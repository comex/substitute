/* define to avoid error that ucontext is "deprecated" (it's unavoidable with
 * sigaction!) */
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "cbit/htab.h"
#include "execmem.h"
#include "darwin/manual-syscall.h"
#include "darwin/mach-decls.h"
#include "substitute.h"
#include "substitute-internal.h"
#include <mach/mach.h>
#ifndef __MigPackStructs
#error wtf
#endif
#include <mach/mig.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <pthread.h>

int manual_sigreturn(void *, int);
GEN_SYSCALL(sigreturn, 184);
__typeof__(mmap) manual_mmap;
GEN_SYSCALL(mmap, 197);
__typeof__(mprotect) manual_mprotect;
GEN_SYSCALL(mprotect, 74);
__typeof__(mach_msg) manual_mach_msg;
GEN_SYSCALL(mach_msg, -31);
__typeof__(mach_thread_self) manual_thread_self;
GEN_SYSCALL(thread_self, -27);

extern int __sigaction(int, struct __sigaction * __restrict, struct sigaction * __restrict);

static void manual_memcpy(void *restrict dest, const void *src, size_t len) {
    /* volatile to avoid compiler transformation to call to memcpy */
    volatile uint8_t *d8 = dest;
    const uint8_t *s8 = src;
    while (len--)
        *d8++ = *s8++;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#define __MachMsgErrorWithTimeout(_R_)
#define __MachMsgErrorWithoutTimeout(_R_)
#include "../generated/manual-mach.inc.h"
#pragma GCC diagnostic pop

#define port_hash(portp) (*(portp))
#define port_eq(port1p, port2p) (*(port1p) == *(port2p))
#define port_null(portp) (*(portp) == MACH_PORT_NULL)
DECL_STATIC_HTAB_KEY(mach_port_t, mach_port_t, port_hash, port_eq, port_null, 0);
struct empty {};
DECL_HTAB(mach_port_set, mach_port_t, struct empty);

/* This should only run on the main thread, so just use globals. */
static HTAB_STORAGE(mach_port_set) g_suspended_ports;
static struct sigaction old_segv, old_bus;
static execmem_pc_patch_callback g_pc_patch_callback;
static void *g_pc_patch_callback_ctx;
static mach_port_t g_suspending_thread;

int execmem_alloc_unsealed(uintptr_t hint, void **page_p, size_t *size_p) {
    *size_p = PAGE_SIZE;
    *page_p = mmap((void *) hint, *size_p, PROT_READ | PROT_WRITE,
                   MAP_ANON | MAP_SHARED, -1, 0);
    if (*page_p == MAP_FAILED)
        return SUBSTITUTE_ERR_VM;
    return SUBSTITUTE_OK;
}

int execmem_seal(void *page) {
    if (mprotect(page, PAGE_SIZE, PROT_READ | PROT_EXEC))
        return SUBSTITUTE_ERR_VM;
    return SUBSTITUTE_OK;
}

void execmem_free(void *page) {
    munmap(page, PAGE_SIZE);
}

#if defined(__x86_64__)
    typedef struct __darwin_x86_thread_state64 native_thread_state;
    #define NATIVE_THREAD_STATE_FLAVOR x86_THREAD_STATE64
#elif defined(__i386__)
    typedef struct __darwin_i386_thread_state native_thread_state;
    #define NATIVE_THREAD_STATE_FLAVOR x86_THREAD_STATE32
#elif defined(__arm__)
    typedef struct __darwin_arm_thread_state native_thread_state;
    #define NATIVE_THREAD_STATE_FLAVOR ARM_THREAD_STATE
#elif defined(__arm64__)
    typedef struct __darwin_arm_thread_state64 native_thread_state;
    #define NATIVE_THREAD_STATE_FLAVOR ARM_THREAD_STATE64
#else
    #error ?
#endif

/* returns whether it changed */
static bool apply_one_pcp_with_state(native_thread_state *state,
                                     execmem_pc_patch_callback callback,
                                     void *ctx) {

    uintptr_t *pcp;
#if defined(__x86_64__)
    pcp = (uintptr_t *) &state->__rip;
#elif defined(__i386__)
    pcp = (uintptr_t *) &state->__eip;
#elif defined(__arm__) || defined(__arm64__)
    pcp = (uintptr_t *) &state->__pc;
#endif
    uintptr_t old = *pcp;
#ifdef __arm__
    /* thumb */
    if (state->__cpsr & 0x20)
        old |= 1;
#endif
    uintptr_t new = callback(ctx, *pcp);
    bool changed = new != old;
    *pcp = new;
#ifdef __arm__
    *pcp &= ~1;
    state->__cpsr = (state->__cpsr & ~0x20) | ((new & 1) * 0x20);
#endif
    return changed;
}

static int apply_one_pcp(mach_port_t thread, execmem_pc_patch_callback callback,
                         void *ctx, mach_port_t reply_port) {
    native_thread_state state;
    mach_msg_type_number_t real_cnt = sizeof(state) / sizeof(int);
    mach_msg_type_number_t cnt = real_cnt;
    kern_return_t kr = manual_thread_get_state(thread, NATIVE_THREAD_STATE_FLAVOR,
                                               (thread_state_t) &state, &cnt,
                                               reply_port);
    if (kr == KERN_TERMINATED)
        return SUBSTITUTE_OK;
    if (kr || cnt != real_cnt)
        return SUBSTITUTE_ERR_ADJUSTING_THREADS;;

    if (apply_one_pcp_with_state(&state, callback, ctx)) {
        kr = manual_thread_set_state(thread, NATIVE_THREAD_STATE_FLAVOR,
                                     (thread_state_t) &state, real_cnt,
                                     reply_port);
        if (kr)
            return SUBSTITUTE_ERR_ADJUSTING_THREADS;
    }
    return SUBSTITUTE_OK;
}

static void resume_other_threads();

static int stop_other_threads() {
    /* pthread_main should have already been checked. */

    int ret;
    mach_port_t self = mach_thread_self();

    /* The following shenanigans are for catching any new threads that are
     * created while we're looping, without suspending anything twice.  Keep
     * looping until only threads we already suspended before this loop are
     * there. */
    HTAB_STORAGE_INIT(&g_suspended_ports, mach_port_set);
    struct htab_mach_port_set *suspended_set = &g_suspended_ports.h;

    bool got_new;
    do {
        got_new = false;

        thread_act_port_array_t ports;
        mach_msg_type_number_t nports;

        kern_return_t kr = task_threads(mach_task_self(), &ports, &nports);
        if (kr) { /* ouch */
            ret = SUBSTITUTE_ERR_ADJUSTING_THREADS;
            goto fail;
        }

        for (mach_msg_type_number_t i = 0; i < nports; i++) {
            mach_port_t port = ports[i];
            struct htab_bucket_mach_port_set *bucket;
            if (port == self ||
                (bucket = htab_setbucket_mach_port_set(suspended_set, &port),
                 bucket->key)) {
                /* already suspended, ignore */
                mach_port_deallocate(mach_task_self(), port);
            } else {
                got_new = true;
                kr = thread_suspend(port);
                if (kr == KERN_TERMINATED) {
                    /* too late */
                    mach_port_deallocate(mach_task_self(), port);
                } else if (kr) {
                    ret = SUBSTITUTE_ERR_ADJUSTING_THREADS;
                    for (; i < nports; i++)
                        mach_port_deallocate(mach_task_self(), ports[i]);
                    vm_deallocate(mach_task_self(), (vm_address_t) ports,
                                  nports * sizeof(*ports));
                    goto fail;
                }
                bucket->key = port;
            }
        }
        vm_deallocate(mach_task_self(), (vm_address_t) ports,
                      nports * sizeof(*ports));
    } while(got_new);

    /* Success - keep the set around for when we're done. */
    return SUBSTITUTE_OK;

fail:
    resume_other_threads();
    return ret;
}

static void resume_other_threads() {
    struct htab_mach_port_set *suspended_set = &g_suspended_ports.h;
    HTAB_FOREACH(suspended_set, mach_port_t *threadp,
                 UNUSED struct empty *_,
                 mach_port_set) {
        thread_resume(*threadp);
        mach_port_deallocate(mach_task_self(), *threadp);
    }
    htab_free_storage_mach_port_set(suspended_set);
}

/* note: unusual prototype since we are avoiding _sigtramp */
static void segfault_handler(UNUSED void *func, int style, int sig,
                             UNUSED siginfo_t *sinfo, void *uap_) {
    ucontext_t *uap = uap_;
    if (manual_thread_self() == g_suspending_thread) {
        /* The patcher itself segfaulted.  Oops.  Reset the signal so the
         * process exits rather than going into an infinite loop. */
        signal(sig, SIG_DFL);
        goto sigreturn;
    }
    /* We didn't catch it before it segfaulted so have to fix it up here. */
    apply_one_pcp_with_state(&uap->uc_mcontext->__ss, g_pc_patch_callback,
                             g_pc_patch_callback_ctx);
    /* just let it continue, whatever */
sigreturn:
    if (manual_sigreturn(uap, style))
        abort();
}

static int init_pc_patch(execmem_pc_patch_callback callback, void *ctx) {
    g_suspending_thread = mach_thread_self();
    g_pc_patch_callback = callback;
    g_pc_patch_callback_ctx = ctx;
    int ret;
    if ((ret = stop_other_threads()))
        return ret;

    struct __sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = (void *) 0xdeadbeef;
    sa.sa_tramp = segfault_handler;
    sigfillset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NODEFER | SA_SIGINFO;

    if (__sigaction(SIGSEGV, &sa, &old_segv))
        return SUBSTITUTE_ERR_ADJUSTING_THREADS;
    if (__sigaction(SIGBUS, &sa, &old_bus)) {
        sigaction(SIGSEGV, &old_segv, NULL);
        return SUBSTITUTE_ERR_ADJUSTING_THREADS;
    }
    return SUBSTITUTE_OK;
}

static int run_pc_patch(mach_port_t reply_port) {
    int ret;

    struct htab_mach_port_set *suspended_set = &g_suspended_ports.h;
    HTAB_FOREACH(suspended_set, mach_port_t *threadp,
                 UNUSED struct empty *_,
                 mach_port_set) {
        if ((ret = apply_one_pcp(*threadp, g_pc_patch_callback,
                                 g_pc_patch_callback_ctx, reply_port)))
            return ret;
    }

    return SUBSTITUTE_OK;
}

static int finish_pc_patch() {
    if (sigaction(SIGBUS, &old_bus, NULL) ||
        sigaction(SIGSEGV, &old_segv, NULL))
        return SUBSTITUTE_ERR_ADJUSTING_THREADS;

    resume_other_threads();
    return SUBSTITUTE_OK;
}

static int compare_dsts(const void *a, const void *b) {
    void *dst_a = ((struct execmem_foreign_write *) a)->dst;
    void *dst_b = ((struct execmem_foreign_write *) b)->dst;
    return dst_a < dst_b ? -1 : dst_a > dst_b ? 1 : 0;
}

static kern_return_t get_page_info(uintptr_t ptr, vm_prot_t *prot_p,
                                   vm_inherit_t *inherit_p) {

    vm_address_t region = (vm_address_t) ptr;
    vm_size_t region_len = 0;
    struct vm_region_submap_short_info_64 info;
    mach_msg_type_number_t info_count = VM_REGION_SUBMAP_SHORT_INFO_COUNT_64;
    natural_t max_depth = 99999;
    kern_return_t kr = vm_region_recurse_64(mach_task_self(), &region, &region_len,
                                            &max_depth,
                                            (vm_region_recurse_info_t) &info,
                                            &info_count);
    *prot_p = info.protection & (PROT_READ | PROT_WRITE | PROT_EXEC);
    *inherit_p = info.inheritance;
    return kr;
}

int execmem_foreign_write_with_pc_patch(struct execmem_foreign_write *writes,
                                        size_t nwrites,
                                        execmem_pc_patch_callback callback,
                                        void *callback_ctx) {
    int ret;

    qsort(writes, nwrites, sizeof(*writes), compare_dsts);

    mach_port_t task_self = mach_task_self();
    mach_port_t reply_port = mig_get_reply_port();

    if (callback) {
        /* Set the segfault handler - stopping all other threads before
         * doing so in case they were using it for something (this
         * happens).  One might think the latter makes segfaults
         * impossible, but we can't prevent injectors from making new
         * threads that might run during this process.  Hopefully no
         * *injected* threads try to use segfault handlers for something!
         */
        if ((ret = init_pc_patch(callback, callback_ctx)))
            return ret;
    }

    size_t last;
    for (size_t first = 0; first < nwrites; first = last + 1) {
        const struct execmem_foreign_write *first_write = &writes[first];
        uintptr_t page_start = (uintptr_t) first_write->dst & ~PAGE_MASK;
        uintptr_t page_end = ((uintptr_t) first_write->dst +
                              first_write->len - 1) & ~PAGE_MASK;

        last = first;
        while (last + 1 < nwrites) {
            const struct execmem_foreign_write *write = &writes[last + 1];
            uintptr_t this_start = (uintptr_t) write->dst & ~PAGE_MASK;
            uintptr_t this_end = ((uintptr_t) write->dst +
                                  first_write->len - 1) & ~PAGE_MASK;
            if (page_start <= this_start && this_start <= page_end) {
                if (this_end > page_end)
                    page_end = this_end;
            } else if (page_start <= this_end && this_end <= page_end) {
                if (this_start < page_start)
                    page_start = this_start;
            } else {
                break;
            }
            last++;
        }
        size_t len = page_end - page_start + PAGE_SIZE;

        vm_prot_t prot;
        vm_inherit_t inherit;
        /* Assume that a single patch region will be pages of all the same
         * protection, since the alternative is probably someone doing
         * something wrong. */
        kern_return_t kr = get_page_info(page_start, &prot, &inherit);
        if (kr) {
            /* Weird; this probably means the region doesn't exist, but we should
             * have already read from the memory in order to generate the patch. */
            ret = SUBSTITUTE_ERR_VM;
            goto fail;
        }
        /* Instead of trying to set the existing region to write, which may
         * fail due to max_protection, we make a fresh copy and remap it over
         * the original. */
        void *new = mmap(NULL, len, PROT_READ | PROT_WRITE,
                         MAP_ANON | MAP_SHARED, -1, 0);
        if (new == MAP_FAILED) {
            ret = SUBSTITUTE_ERR_VM;
            goto fail;
        }
        /* Ideally, if the original page wasn't mapped anywhere else, no actual
         * copy will take place: new will be CoW, then we unmap the original so
         * new becomes the sole owner before actually writing.  Though, for all
         * I know, these trips through the VM system could be slower than just
         * memcpying a page or two... */
        kr = vm_copy(task_self, page_start, len, (vm_address_t) new);
        if (kr) {
            ret = SUBSTITUTE_ERR_VM;
            goto fail_unmap;
        }
        /* Start of danger zone: between the mmap PROT_NONE and remap, we avoid
         * using any standard library functions in case the user is trying to
         * hook one of them.  (This includes the mmap, since there's an epilog
         * after the actual syscall instruction.)
         * This includes the signal handler! */
        void *mmret = manual_mmap((void *) page_start, len, PROT_NONE,
                                  MAP_ANON | MAP_SHARED | MAP_FIXED, -1, 0);
        /* MAP_FAILED is a userspace construct */
        if ((uintptr_t) mmret & 0xfff) {
            ret = SUBSTITUTE_ERR_VM;
            goto fail_unmap;
        }
        /* Write patches to the copy. */
        for (size_t i = first; i <= last; i++) {
            struct execmem_foreign_write *write = &writes[i];
            ptrdiff_t off = (uintptr_t) write->dst - page_start;
            manual_memcpy(new + off, write->src, write->len);
        }
        if (callback) {
            /* Actually run the callback for any threads which are paused at an
             * affected PC, or are running and don't get scheduled by the
             * kernel in time to segfault.  Any thread which moves to an
             * affected PC *after* run_pc_patch() is assumed to do so by
             * calling the function in question, so they can't get past the
             * first instruction and it doesn't matter whether or not they're
             * patched.  (A call instruction within the affected region would
             * break this assumption, as then a thread could move to an
             * affected PC by returning. */
            if ((ret = run_pc_patch(reply_port)))
                goto fail_unmap;
        }

        /* Protect new like the original, and move it into place. */
        if (manual_mprotect(new, len, prot)) {
            ret = SUBSTITUTE_ERR_VM;
            goto fail_unmap;
        }
        vm_prot_t c, m;
        mach_vm_address_t target = page_start;
        kr = manual_mach_vm_remap(mach_task_self(), &target, len, 0,
                                  VM_FLAGS_OVERWRITE, task_self,
                                  (mach_vm_address_t) new, /*copy*/ TRUE,
                                  &c, &m, inherit, reply_port);
        if (kr) {
            ret = SUBSTITUTE_ERR_VM;
            goto fail_unmap;
        }
        /* Danger zone over.  Ignore errors when unmapping the temporary buffer. */
        munmap(new, len);

        continue;

    fail_unmap:
        /* This is probably useless, since the original page is gone
         * forever (intentionally, see above).  May as well arrange the
         * deck chairs, though. */
        munmap(new, len);
        goto fail;
    }

    ret = 0;

fail:
    if (callback) {
        /* Other threads are no longer in danger of segfaulting, so put
         * back the old segfault handler. */
        int ret2;
        if ((ret2 = finish_pc_patch()))
            return ret2;
    }

    return ret;
}

