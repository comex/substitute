#ifdef __APPLE__
#include "substitute.h"
#include "substitute-internal.h"
#include "darwin/read.h"
#include <mach/mach.h>
#include <sys/param.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

kern_return_t mach_vm_read_overwrite(vm_map_t, mach_vm_address_t, mach_vm_size_t, mach_vm_address_t, mach_vm_size_t *);
kern_return_t mach_vm_remap(vm_map_t, mach_vm_address_t *, mach_vm_size_t, mach_vm_offset_t, int, vm_map_t, mach_vm_address_t, boolean_t, vm_prot_t *, vm_prot_t *, vm_inherit_t);

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

static int find_foreign_images(mach_port_t task, uint64_t *libdyld_p, uint64_t *libpthread_p, char **error) {
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
    mach_vm_size_t size;
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
        kr = mach_vm_read_overwrite(task, file_path, toread,
                                    (mach_vm_address_t) path_buf, &size);
        if (kr) {
            printf("kr=%d <%p %p>\n", kr, (void *) file_path, path_buf);
            continue;
        }
        if (strlen(path_buf) == toread && toread < MAXPATHLEN) {
            /* get the rest... */
            kr = mach_vm_read_overwrite(task, file_path + toread, MAXPATHLEN - toread,
                                        (mach_vm_address_t) path_buf + toread, &size);
            if (kr) {
                continue;
            }
            path_buf[MAXPATHLEN] = '\0';
        }

        if (!strcmp(path_buf, "/usr/lib/system/libdyld.dylib"))
            *libdyld_p = load_address;
        else if (!strcmp(path_buf, "/usr/lib/system/libpthread.dylib"))
            *libpthread_p = load_address;

        if (*libdyld_p && *libpthread_p)
            return SUBSTITUTE_OK;

        info_array_ptr += info_array_elm_size;
    }

    asprintf(error, "couldn't find libpthread");
    return SUBSTITUTE_ERR_MISC;
}

static int get_foreign_image_export(mach_port_t task, uint64_t hdr_addr,
                                    void **linkedit_p, size_t *linkedit_size_p,
                                    void **export_p, size_t *export_size_p,
                                    char **error) {
    mach_vm_offset_t hdr_buf;
    mach_vm_size_t hdr_buf_size;
    int ret;
    if (hdr_addr & (PAGE_SIZE - 1)) {
        asprintf(error, "unaligned mach_header");
        return SUBSTITUTE_ERR_MISC;
    }

    vm_prot_t cur, max;
    hdr_buf_size = PAGE_SIZE;
    kern_return_t kr = mach_vm_remap(mach_task_self(), &hdr_buf, hdr_buf_size, 0,
                                     VM_FLAGS_ANYWHERE, task, hdr_addr, /*copy*/ true,
                                     &cur, &max, VM_INHERIT_NONE);
    if (kr) {
        asprintf(error, "mach_vm_remap(libdyld header): kr=%d", kr);
        return SUBSTITUTE_ERR_MISC;
    }

    struct mach_header *mh = (void *) hdr_buf;
    if (mh->magic != MH_MAGIC && mh->magic != MH_MAGIC_64) {
        asprintf(error, "bad magic in libdyld mach_header");
        ret = SUBSTITUTE_ERR_MISC;
        goto fail;
    }

    size_t mh_size = mh->magic == MH_MAGIC_64 ? sizeof(struct mach_header_64)
                                              : sizeof(struct mach_header);
    if (mh->sizeofcmds < mh_size || mh->sizeofcmds > 128*1024)
        goto badmach;

    size_t total_size = mh_size + mh->sizeofcmds;
    if (total_size > hdr_buf_size) {
        vm_deallocate(mach_task_self(), (vm_offset_t) hdr_buf, (vm_size_t) hdr_buf_size);
        hdr_buf_size = total_size;
        kr = mach_vm_remap(mach_task_self(), &hdr_buf, hdr_buf_size, 0,
                           VM_FLAGS_ANYWHERE, task, hdr_addr, /*copy*/ true,
                           &cur, &max, VM_INHERIT_NONE);
        if (kr) {
            asprintf(error, "mach_vm_remap(libdyld header) #2: kr=%d", kr);
            ret = SUBSTITUTE_ERR_MISC;
            goto fail;
        }
        mh = (void *) hdr_buf;
    }

    struct load_command *lc = (void *) mh + mh_size;
    uint32_t export_off = 0, export_size = 0;
    uint64_t slide = 0;
    for (uint32_t i = 0; i < mh->ncmds; i++, lc = (void *) lc + lc->cmdsize) {
        size_t remaining = total_size - ((void *) lc - (void *) mh);
        if (remaining < sizeof(*lc) || remaining < lc->cmdsize)
            goto badmach;
        if (lc->cmd == LC_DYLD_INFO || lc->cmd == LC_DYLD_INFO_ONLY) {
            struct dyld_info_command *dc = (void *) lc;
            if (lc->cmdsize < sizeof(*dc))
                goto badmach;
            export_off = dc->export_off;
            export_size = dc->export_size;
        } else if (lc->cmd == LC_SEGMENT) {
            struct segment_command *sc = (void *) lc;
            if (lc->cmdsize < sizeof(*sc))
                goto badmach;
            if (sc->fileoff == 0)
                slide = hdr_addr - sc->vmaddr;
        } else if (lc->cmd == LC_SEGMENT_64) {
            struct segment_command_64 *sc = (void *) lc;
            if (lc->cmdsize < sizeof(*sc))
                goto badmach;
            if (sc->fileoff == 0)
                slide = hdr_addr - sc->vmaddr;
        }
    }

    if (export_off == 0) {
        asprintf(error, "no LC_DYLD_INFO in libdyld header");
        ret = SUBSTITUTE_ERR_MISC;
        goto fail;
    }
    lc = (void *) mh + mh_size;


    uint64_t export_segoff, vmaddr, fileoff, filesize;
    for (uint32_t i = 0; i < mh->ncmds; i++, lc = (void *) lc + lc->cmdsize) {
        if (lc->cmd == LC_SEGMENT) {
            struct segment_command *sc = (void *) lc;
            vmaddr = sc->vmaddr;
            fileoff = sc->fileoff;
            filesize = sc->filesize;
        } else if (lc->cmd == LC_SEGMENT_64) {
            struct segment_command_64 *sc = (void *) lc;
            vmaddr = sc->vmaddr;
            fileoff = sc->fileoff;
            filesize = sc->filesize;
        } else {
            continue;
        }
        export_segoff = (uint64_t) export_off - fileoff;
        if (export_segoff < filesize) {
            if (export_size > filesize - export_segoff)
                goto badmach;
            break;
        }
    }

    uint64_t linkedit_addr = vmaddr + slide;
    mach_vm_address_t linkedit_buf;
    kr = mach_vm_remap(mach_task_self(), &linkedit_buf, filesize, 0,
                       VM_FLAGS_ANYWHERE, task, linkedit_addr, /*copy*/ true,
                       &cur, &max, VM_INHERIT_NONE);
    if (kr) {
        asprintf(error, "mach_vm_remap(libdyld linkedit): kr=%d", kr);
        ret = SUBSTITUTE_ERR_MISC;
        goto fail;
    }

    *linkedit_p = (void *) linkedit_buf;
    *linkedit_size_p = (size_t) filesize;
    *export_p = (void *) linkedit_buf + export_segoff;
    *export_size_p = export_size;

    ret = SUBSTITUTE_OK;
    goto fail;

badmach:
    asprintf(error, "bad Mach-O data in libdyld header");
    ret = SUBSTITUTE_ERR_MISC;
    goto fail;
fail:
    vm_deallocate(mach_task_self(), (vm_offset_t) hdr_buf, (vm_size_t) hdr_buf_size);
    return ret;
}

static bool find_export_symbol(void *export, size_t export_size, const char *name,
                               uint64_t hdr_addr, uint64_t *sym_addr_p) {
    void *end = export + export_size;
    void *ptr = export;
    while (1) {
        /* skip this symbol data */
        uint64_t size;
        if (!read_leb128(&ptr, end, false, &size) ||
            size > (uint64_t) (end - ptr))
            return false;
        ptr += size;
        if (ptr == end)
            return false;
        uint8_t i, nedges = *(uint8_t *) ptr;
        ptr++;
        for (i = 0; i < nedges; i++) {
            char *prefix;
            if (!read_cstring(&ptr, end, &prefix))
                return false;
            size_t prefix_len = (char *) ptr - prefix - 1;
            uint64_t next_offset;
            if (!read_leb128(&ptr, end, false, &next_offset))
                return false;
            if (!strncmp(name, prefix, prefix_len)) {
                if (next_offset > export_size)
                    return false;
                ptr = export + next_offset;
                name += prefix_len;
                if (*name == '\0')
                    goto got_symbol;
                break;
            }
        }
        if (i == nedges) {
            /* not found */
            return false;
        }
    }
got_symbol:;
    uint64_t size, flags, hdr_off;
    if (!read_leb128(&ptr, end, false, &size))
        return false;
    if (!read_leb128(&ptr, end, false, &flags))
        return false;
    if (flags & (EXPORT_SYMBOL_FLAGS_REEXPORT | EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER)) {
        /* don't bother to support for now */
        return false;
    }
    if (!read_leb128(&ptr, end, false, &hdr_off))
        return false;
    *sym_addr_p = hdr_addr + hdr_off;
    return true;
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

    uint64_t libdyld_addr, libpthread_addr;
    if ((ret = find_foreign_images(task, &libdyld_addr, &libpthread_addr, error)))
        goto fail;

    struct {
        uint64_t addr;
        const char *symname;
        uint64_t symaddr;
    } libs[2] = {
        {libdyld_addr, "_dlopen", 0},
        {libpthread_addr, "_pthread_create", 0}
    };

    for (int i = 0; i < 2; i++) {
        void *linkedit, *export;
        size_t linkedit_size, export_size;
        if ((ret = get_foreign_image_export(task, libs[i].addr,
                                            &linkedit, &linkedit_size,
                                            &export, &export_size,
                                            error)))
            goto fail;
        printf("%p\n", (void *) libdyld_addr);
        bool fesr = find_export_symbol(export, export_size, libs[i].symname,
                                       libs[i].addr, &libs[i].symaddr);
        vm_deallocate(mach_task_self(), (vm_offset_t) linkedit, (vm_size_t) linkedit_size);
        if (!fesr) {
            asprintf(error, "couldn't find _dlopen in libdyld");
            ret = SUBSTITUTE_ERR_MISC;
            goto fail;
        }
    }
    printf("%p\n", (void *) libs[0].symaddr);
    printf("%p\n", (void *) libs[0].symaddr);

    (void) filename;
    (void) options;

    ret = 0;
fail:
    mach_port_deallocate(mach_task_self(), task);
    return ret;
}
#endif /* __APPLE__ */
