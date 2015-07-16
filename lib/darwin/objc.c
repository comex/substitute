#if defined(__APPLE__)
#include "substitute.h"
#include "substitute-internal.h"
#include "objc.h"
#include <stddef.h>
#include <pthread.h>
#include <mach/mach.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <errno.h>

/* These trampolines will call e->func(e->arg1, e->arg2), and jump to there,
 * preserving all arguments.  imp_implementationWithBlock would be easier and
 * maybe a bit faster, but it's impossible to avoid throwing away registers
 * without having to ask whether the selector is stret or not. */

struct tramp_info_page_header {
    uint32_t magic;
    uint32_t version;
    struct tramp_info_page_entry *first_free;
    size_t nfree;
    LIST_ENTRY(tramp_info_page_header) free_pages;
};

enum {
    TRAMP_MAGIC = 0xf00df17e,
    TRAMP_VERSION = 0,
};

struct tramp_info_page_entry {
    union {
        struct tramp_info_page_entry *next_free;
        void *func;
    };
    void *arg1;
    void *arg2;
};

_Static_assert(TRAMP_INFO_PAGE_ENTRY_SIZE == sizeof(struct tramp_info_page_entry),
               "TRAMP_INFO_PAGE_ENTRY_SIZE");
_Static_assert(sizeof(struct tramp_info_page_header) +
               TRAMPOLINES_PER_PAGE * sizeof(struct tramp_info_page_entry)
               <= _PAGE_SIZE,
               "header+entries too big");

static pthread_mutex_t tramp_mutex = PTHREAD_MUTEX_INITIALIZER;
LIST_HEAD(tramp_info_page_list, tramp_info_page_header)
    tramp_free_page_list = LIST_HEAD_INITIALIZER(tramp_info_page_list);

extern char remap_start[];

static int get_trampoline(void *func, void *arg1, void *arg2, void *tramp_ptr) {
    int ret, rerrno = 0;
    pthread_mutex_lock(&tramp_mutex);

    struct tramp_info_page_header *header = LIST_FIRST(&tramp_free_page_list);
    if (!header) {
        if (PAGE_SIZE > _PAGE_SIZE)
            substitute_panic("%s: strange PAGE_SIZE %lx\n",
                             __func__, (long) PAGE_SIZE);
        void *new_pages = mmap(NULL, _PAGE_SIZE * 2, PROT_READ | PROT_WRITE,
                               MAP_SHARED | MAP_ANON, -1, 0);
        if (new_pages == MAP_FAILED) {
            ret = SUBSTITUTE_ERR_OOM;
            rerrno = errno;
            goto out;
        }
        vm_address_t tramp_page = (vm_address_t) new_pages;
        vm_prot_t cur_prot, max_prot;
        kern_return_t kr = vm_remap(
            mach_task_self(),
            &tramp_page,
            _PAGE_SIZE,
            _PAGE_SIZE - 1,
            VM_FLAGS_OVERWRITE | VM_FLAGS_FIXED,
            mach_task_self(),
            (vm_address_t) remap_start,
            FALSE, /* copy */
            &cur_prot,
            &max_prot,
            VM_INHERIT_NONE);
        if (kr != KERN_SUCCESS || tramp_page != (vm_address_t) new_pages) {
            ret = SUBSTITUTE_ERR_VM;
            goto out;
        }
        header = new_pages + _PAGE_SIZE * 2 - sizeof(*header);
        header->magic = TRAMP_MAGIC;
        header->version = TRAMP_VERSION;
        header->first_free = NULL;
        header->nfree = TRAMPOLINES_PER_PAGE;
        LIST_INSERT_HEAD(&tramp_free_page_list, header, free_pages);
    }

    void *page = (void *) (((uintptr_t) header) & ~(_PAGE_SIZE - 1));
    struct tramp_info_page_entry *entries = page;
    struct tramp_info_page_entry *entry = header->first_free;
    if (entry == NULL) {
        entry = &entries[TRAMPOLINES_PER_PAGE - header->nfree];
        entry->next_free = NULL;
    }

    header->first_free = entry->next_free;
    if (--header->nfree == 0)
        LIST_REMOVE(header, free_pages);

    entry->func = func;
    entry->arg1 = arg1;
    entry->arg2 = arg2;
    void *tramp = (page - PAGE_SIZE) + (entry - entries) * TRAMPOLINE_SIZE;
#ifdef __arm__
    tramp += 1;
#endif
    *(void **) tramp_ptr = tramp;
    ret = SUBSTITUTE_OK;
out:
    pthread_mutex_unlock(&tramp_mutex);
    errno = rerrno;
    return ret;
}

static void free_trampoline(void *tramp) {
    pthread_mutex_lock(&tramp_mutex);
    void *page = (void *) (((uintptr_t) tramp) & ~(_PAGE_SIZE - 1));
    size_t i = (tramp - page) / TRAMPOLINE_SIZE;
    struct tramp_info_page_entry *entries = page + _PAGE_SIZE;
    struct tramp_info_page_entry *entry = &entries[i];
    struct tramp_info_page_header *header = page + 2 * _PAGE_SIZE - sizeof(*header);

    if (header->magic != TRAMP_MAGIC)
        substitute_panic("%s: bad pointer\n", __func__);
    if (header->version != TRAMP_VERSION) {
        /* shouldn't happen, but just in case multiple versions of this library
         * are mixed up */
        return;
    }

    entry->next_free = header->first_free;
    header->first_free = entry;
    header->nfree++;
    if (header->nfree == 1)
        LIST_INSERT_HEAD(&tramp_free_page_list, header, free_pages);
    else if (header->nfree == TRAMPOLINES_PER_PAGE &&
        /* have others? */
        (LIST_FIRST(&tramp_free_page_list) != header ||
         LIST_NEXT(header, free_pages))) {
        /* free the trampoline and info pages */
        LIST_REMOVE(header, free_pages);
        munmap(page, 2 * _PAGE_SIZE);
    }

    pthread_mutex_unlock(&tramp_mutex);
}

static IMP dereference(IMP *old_ptr, UNUSED void *_) {
    return *old_ptr;
}

EXPORT
int substitute_hook_objc_message(Class class, SEL selector, void *replacement,
                                 void *old_ptr, bool *created_imp_ptr) {
    int ret;
    Method meth = class_getInstanceMethod(class, selector);
    if (meth == NULL)
        return SUBSTITUTE_ERR_NO_SUCH_SELECTOR;
    const char *types = method_getTypeEncoding(meth);

    if (created_imp_ptr)
        *created_imp_ptr = false;

    /* temporary trampoline just tries again */
    IMP temp = NULL;
    if (old_ptr) {
        if ((ret = get_trampoline(dereference, old_ptr, NULL, &temp)))
            return ret;
        *(IMP *) old_ptr = temp;
    }

    IMP old = class_replaceMethod(class, selector, replacement, types);
    if (old) {
        if (old_ptr)
            *(IMP *) old_ptr = old;
    } else {
        if (old_ptr) {
            Class super = class_getSuperclass(class);
            if (!super) {
                /* this ought to only be possible if the method was removed in
                 * the meantime, since we found the method above and it
                 * couldn't have been found in a superclass, but the objc2
                 * runtime doesn't allow removing methods. */
                substitute_panic("%s: no superclass but the method didn't exist\n",
                                 __func__);
            }
            ret = get_trampoline(class_getMethodImplementation, super,
                                 selector, old_ptr);
            if (created_imp_ptr)
                *created_imp_ptr = true;
        }
    }

    if (temp)
        free_trampoline(temp);
    return SUBSTITUTE_OK;
}

EXPORT
void substitute_free_created_imp(IMP imp) {
    free_trampoline(imp);
}
#endif
