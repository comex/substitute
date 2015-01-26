#include "execmem.h"
#include "darwin/manual-syscall.h"
#include "substitute.h"
#include <mach/mach.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <errno.h>
#include <stdio.h>

int execmem_write(void *dest, const void *src, size_t len) {
    /* Use vm_region to determine the original protection, so we can mprotect
     * it back afterwards.  (Note: PROT_* are equal to VM_PROT_*.) */
    vm_address_t region = (vm_address_t) dest;
    vm_size_t region_len = 0;
    struct vm_region_submap_short_info_64 info;
    mach_msg_type_number_t info_count = VM_REGION_SUBMAP_SHORT_INFO_COUNT_64;
    natural_t max_depth = 99999;
    kern_return_t kr = vm_region_recurse_64(mach_task_self(), &region, &region_len,
                                            &max_depth,
                                            (vm_region_recurse_info_t) &info,
                                            &info_count);
    if (kr) {
        /* Weird; this probably means the region doesn't exist, but we should
         * have already read from the memory in order to generate the patch. */
        errno = 0;
        return SUBSTITUTE_ERR_VM;
    }

    uintptr_t lopage = (uintptr_t) dest & ~PAGE_MASK;
    uintptr_t hipage = ((uintptr_t) dest + len + PAGE_MASK) & ~PAGE_MASK;

    /* We do the syscall manually just in case the user is trying to write to
     * the mprotect syscall stub itself, or one of the functions it calls.
     * (Obviously, it will still break if the user targets some libsubstitute
     * function within the same page as this one, though.) */
    int ret = manual_syscall(SYS_mprotect, lopage, hipage - lopage,
                             PROT_READ | PROT_WRITE, 0);
    if (ret) {
        errno = ret;
        return SUBSTITUTE_ERR_VM;
    }

    /* volatile to avoid compiler transformation to call to memcpy */
    volatile uint8_t *d8 = dest;
    const uint8_t *s8 = src;
    while (len--)
        *d8++ = *s8++;

    int oldprot = info.protection & (PROT_READ | PROT_WRITE | PROT_EXEC);
    ret = manual_syscall(SYS_mprotect, lopage, hipage - lopage,
                         oldprot, 0);
    if (ret) {
        errno = ret;
        return SUBSTITUTE_ERR_VM;
    }
    return SUBSTITUTE_OK;
}

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
