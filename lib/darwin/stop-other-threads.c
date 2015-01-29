#if 0
#include "substitute.h"
#include "substitute-internal.h"
#include "darwin/mach-decls.h"
#include <pthread.h>
#include <mach/mach.h>

static void release_port(UNUSED CFAllocatorRef allocator, const void *value) {
    mach_port_t thread = (mach_port_t) value;
    thread_resume(thread);
    mach_port_deallocate(mach_task_self(), thread);
}
static CFSetCallBacks suspend_port_callbacks = {
    .version = 0,
    .release = release_port,
};

static bool apply_one_pcp(mach_port_t thread,
                          uintptr_t (*callback)(void *ctx, uintptr_t pc),
                          void *ctx) {
    int flavor;
#if defined(__x86_64__)
    struct _x86_thread_state_64 state;
    flavor = _x86_thread_state_64_flavor;
#elif defined(__i386__)
    struct _x86_thread_state_32 state;
    flavor = _x86_thread_state_32_flavor;
#elif defined(__arm__)
    struct _arm_thread_state_32 state;
    flavor = _arm_thread_state_32_flavor;
#elif defined(__arm64__)
    struct _arm_thread_state_64 state;
    flavor = _arm_thread_state_64_flavor;
#else
    #error ?
#endif

    mach_msg_type_number_t real_cnt = sizeof(state) / sizeof(int);
    mach_msg_type_number_t cnt = real_cnt;
    kern_return_t kr = thread_get_state(thread, flavor, (thread_state_t) &state, &cnt);
    if (kr || cnt != real_cnt)
        return false;

    uintptr_t *pcp;
#if defined(__x86_64__)
    pcp = (uintptr_t *) &state.rip;
#elif defined(__i386__)
    pcp = (uintptr_t *) &state.eip;
#elif defined(__arm__) || defined(__arm64__)
    pcp = (uintptr_t *) &state.pc;
#endif
    uintptr_t old = *pcp;
#ifdef __arm__
    /* thumb */
    if (state.cpsr & 0x20)
        old |= 1;
#endif
    uintptr_t new = callback(ctx, *pcp);
    if (new != old) {
        *pcp = new;
#ifdef __arm__
        *pcp &= ~1;
        state.cpsr = (state.cpsr & ~0x20) | ((new & 1) * 0x20);
#endif
        kr = thread_set_state(thread, flavor, (thread_state_t) &state, real_cnt);
        if (kr)
            return false;
    }
    return true;
}

int apply_pc_patch_callback(void *token,
                            uintptr_t (*pc_patch_callback)(void *ctx, uintptr_t pc),
                            void *ctx) {
    CFMutableSetRef suspended_set = token;
    CFIndex count = CFSetGetCount(suspended_set);
    if (!count)
        return SUBSTITUTE_OK;
    /* great API there CF */
    const void **ports = malloc(sizeof(*ports) * count);
    CFSetGetValues(suspended_set, ports);
    int ret = SUBSTITUTE_OK;
    for (CFIndex i = 0; i < count; i++) {
        if (!apply_one_pcp((mach_port_t) ports[i], pc_patch_callback, ctx)) {
            ret = SUBSTITUTE_ERR_ADJUSTING_THREADS;
            break;
        }
    }
    free(ports);
    return ret;
}

int stop_other_threads(void **token_ptr) {
    if (!pthread_main_np())
        return SUBSTITUTE_ERR_NOT_ON_MAIN_THREAD;

    int ret;
    mach_port_t self = mach_thread_self();

    /* The following shenanigans are for catching any new threads that are
     * created while we're looping, without suspending anything twice.  Keep
     * looping until only threads we already suspended before this loop are
     * there. */
    CFMutableSetRef suspended_set = CFSetCreateMutable(NULL, 0, &suspend_port_callbacks);

    thread_act_array_t ports = 0;
    mach_msg_type_number_t nports = 0;

    bool got_new = true;
    while (got_new) {
        got_new = false;

        kern_return_t kr = task_threads(mach_task_self(), &ports, &nports);
        if (kr) { /* ouch */
            ret = SUBSTITUTE_ERR_ADJUSTING_THREADS;
            goto fail;
        }

        for (mach_msg_type_number_t i = 0; i < nports; i++) {
            mach_port_t port = ports[i];
            void *casted_port = (void *) (uintptr_t) port;
            if (port == self ||
                CFSetContainsValue(suspended_set, casted_port)) {
                /* already suspended, ignore */
                mach_port_deallocate(mach_task_self(), port);
            } else {
                got_new = true;
                printf("suspending %d (self=%d)\n", port, self);
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
                CFSetAddValue(suspended_set, casted_port);
            }
        }
        vm_deallocate(mach_task_self(), (vm_address_t) ports,
                      nports * sizeof(*ports));
    }

    /* Success - keep the set around for when we're done. */
    *token_ptr = suspended_set;
    return SUBSTITUTE_OK;

fail:
    CFRelease(suspended_set);
    return ret;
}

int resume_other_threads(void *token) {
    CFMutableSetRef suspended_set = token;
    CFRelease(suspended_set);
    return SUBSTITUTE_OK; /* eh */
}
#endif
