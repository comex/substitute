#ifdef __APPLE__
#include "substitute.h"
#include "substitute-internal.h"
#include <mach/mach.h>
#include <sys/param.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

kern_return_t mach_vm_read_overwrite(vm_map_t, mach_vm_address_t, mach_vm_size_t, mach_vm_address_t, mach_vm_size_t *);

#define DEFINE_STRUCTS

#define dyld_image_infos_fields(ptr) \
    uint32_t version; \
    uint32_t infoArrayCount; \
    ptr infoArray; \
    ptr notification; \
    bool processDetachedFromSharedRegion; \
    bool libSystemInitialized; \
    ptr dyldImageLoadAddress; \
    ptr jitInfo; \
    ptr dyldVersion; \
    ptr errorMessage; \
    ptr terminationFlags; \
    ptr coreSymbolicationShmPage; \
    ptr systemOrderFlag; \
    ptr uuidArrayCount; \
    ptr uuidArray; \
    ptr dyldAllImageInfosAddress; \
    ptr initialImageCount; \
    ptr errorKind; \
    ptr errorClientOfDylibPath; \
    ptr errorTargetDylibPath; \
    ptr errorSymbol; \
    ptr sharedCacheSlide; \
    uint8_t sharedCacheUUID[16]; \
    ptr reserved[16];

struct dyld_all_image_infos_32 {
    dyld_image_infos_fields(uint32_t)
};
struct dyld_all_image_infos_64 {
    dyld_image_infos_fields(uint64_t)
};

static int find_libs_in_task(mach_port_t task, uint64_t *dyld_addr_p,
                             uint64_t *libpthread_p, char **error) {
    struct task_dyld_info tdi;
    mach_msg_type_number_t cnt = TASK_DYLD_INFO_COUNT;

    kern_return_t kr = task_info(task, TASK_DYLD_INFO, (void *) &tdi, &cnt);
    if (kr || cnt != TASK_DYLD_INFO_COUNT) {
        asprintf(error, "task_info(TASK_DYLD_INFO): kr=%d", kr);
        return SUBSTITUTE_ERR_MISC;
    }

    if (!tdi.all_image_info_addr || !tdi.all_image_info_size ||
        tdi.all_image_info_size > 1024 ||
        tdi.all_image_info_format > TASK_DYLD_ALL_IMAGE_INFO_64) {
        asprintf(error, "TASK_DYLD_INFO obviously malformed");
        return SUBSTITUTE_ERR_MISC;
    }
    char all_image_infos_buf[1024];

    cnt = tdi.all_image_info_size;
    mach_vm_size_t size = tdi.all_image_info_size;
    kr = mach_vm_read_overwrite(task, tdi.all_image_info_addr, tdi.all_image_info_size,
                                (mach_vm_address_t) all_image_infos_buf, &size);
    if (kr || size != tdi.all_image_info_size) {
        asprintf(error, "mach_vm_read_overwrite(all_image_info): kr=%d", kr);
        return SUBSTITUTE_ERR_MISC;
    }

    bool is64 = tdi.all_image_info_format = TASK_DYLD_ALL_IMAGE_INFO_64;
    const struct dyld_all_image_infos_32 *aii32 = (void *) all_image_infos_buf;
    const struct dyld_all_image_infos_64 *aii64 = (void *) all_image_infos_buf;

    #define FIELD(f) (is64 ? aii64->f : aii32->f)

    if (FIELD(version) < 2) {
        /* apparently we're on Leopard or something */
        asprintf(error, "dyld_all_image_infos version too low");
        return SUBSTITUTE_ERR_MISC;
    }

    *dyld_addr_p = FIELD(dyldImageLoadAddress);

    uint64_t info_array_addr = FIELD(infoArray);
    uint32_t info_array_count = FIELD(infoArrayCount);
    size_t info_array_elm_size = (is64 ? sizeof(uint64_t) : sizeof(uint32_t)) * 3;

    #undef FIELD

    if (info_array_count > 2000) {
        asprintf(error, "unreasonable number of loaded libraries: %u", info_array_count);
        return SUBSTITUTE_ERR_MISC;
    }

    size_t info_array_size = info_array_count * info_array_elm_size;
    void *info_array = malloc(info_array_count * info_array_elm_size);
    if (!info_array)
        return SUBSTITUTE_ERR_OOM;

    kr = mach_vm_read_overwrite(task, info_array_addr, info_array_size,
                                (mach_vm_address_t) info_array, &size);
    if (kr || size != info_array_size) {
        asprintf(error, "mach_vm_read_overwrite(info_array): kr=%d", kr);
        return SUBSTITUTE_ERR_MISC;
    }

    /* yay, slow file path reads! */

    void *info_array_ptr = info_array;
    for (uint32_t i = 0; i < info_array_count; i++) {
        uint64_t load_address;
        uint64_t file_path;
        if (is64) {
            uint64_t *e = info_array_ptr;
            load_address = e[0];
            file_path = e[1];
        } else {
            uint32_t *e = info_array_ptr;
            load_address = e[0];
            file_path = e[1];
        }

        /* mach_vm_read_overwrite won't do partial copies, so... */

        char path_buf[MAXPATHLEN+1];
        size_t toread = MIN(MAXPATHLEN, -file_path & 0xfff);
        path_buf[toread] = '\0';
        kr = mach_vm_read_overwrite(task, (mach_vm_address_t) path_buf, toread,
                                    load_address, &size);
        if (kr) {
            continue;
        }
        if (strlen(path_buf) == toread && toread < MAXPATHLEN) {
            /* get the rest... */
            kr = mach_vm_read_overwrite(task, (mach_vm_address_t) path_buf + toread,
                                        MAXPATHLEN - toread, load_address + toread,
                                        &size);
            if (kr) {
                continue;
            }
            path_buf[MAXPATHLEN] = '\0';
        }

        if (!strcmp(path_buf, "/usr/lib/libpthread.dylib")) {
            *libpthread_p = load_address;
            return 0;
        }

        info_array_ptr += info_array_elm_size;
    }

    asprintf(error, "couldn't find libpthread");
    return SUBSTITUTE_ERR_MISC;
}

EXPORT
int substitute_dlopen_in_pid(int pid, const char *filename, int options, char **error) {
    mach_port_t task;
    *error = NULL;
    kern_return_t kr = task_for_pid(mach_task_self(), pid, &task);
    int ret;
    if (kr) {
        asprintf(error, "task_for_pid: kr=%d", kr);
        return SUBSTITUTE_ERR_TASK_FOR_PID;
    }

    uint64_t dyld_addr, libpthread_addr;
    if ((ret = find_libs_in_task(task, &dyld_addr, &libpthread_addr, error)))
        goto fail;
    (void) filename;
    (void) options;
    printf("%p %p\n", (void *) dyld_addr, (void *) libpthread_addr);

    ret = 0;
fail:
    mach_port_deallocate(mach_task_self(), task);
    return ret;
}
#endif /* __APPLE__ */
