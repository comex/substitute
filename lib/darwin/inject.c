#ifdef __APPLE__
#include "substitute.h"
#include "substitute-internal.h"
#include "darwin/read.h"
#include "darwin/thread-state.h"
#include <mach/mach.h>
#include <mach-o/dyld_images.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/param.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

kern_return_t mach_vm_read_overwrite(vm_map_t, mach_vm_address_t, mach_vm_size_t, mach_vm_address_t, mach_vm_size_t *);
kern_return_t mach_vm_remap(vm_map_t, mach_vm_address_t *, mach_vm_size_t, mach_vm_offset_t, int, vm_map_t, mach_vm_address_t, boolean_t, vm_prot_t *, vm_prot_t *, vm_inherit_t);
kern_return_t mach_vm_write(vm_map_t, mach_vm_address_t, vm_offset_t, mach_msg_type_number_t);
kern_return_t mach_vm_allocate(vm_map_t, mach_vm_address_t *, mach_vm_size_t, int);
kern_return_t mach_vm_deallocate(vm_map_t, mach_vm_address_t, mach_vm_size_t);

extern const struct dyld_all_image_infos *_dyld_get_all_image_infos();

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

#define FFI_SHORT_CIRCUIT -1

static int find_foreign_images(mach_port_t task, uint64_t *libdyld_p, uint64_t *libpthread_p, char **error) {
    *libdyld_p = 0;
    *libpthread_p = 0;

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

    bool is64 = tdi.all_image_info_format == TASK_DYLD_ALL_IMAGE_INFO_64;
    const struct dyld_all_image_infos_32 *aii32 = (void *) all_image_infos_buf;
    const struct dyld_all_image_infos_64 *aii64 = (void *) all_image_infos_buf;

    #define FIELD(f) (is64 ? aii64->f : aii32->f)

    if (FIELD(version) < 2) {
        /* apparently we're on Leopard or something */
        asprintf(error, "dyld_all_image_infos version too low");
        return SUBSTITUTE_ERR_MISC;
    }

    /* If we are on the same shared cache with the same slide, then we can just
     * look up the symbols locally and don't have to do the rest of the
     * syscalls... not sure if this is any faster, but whatever. */
    if (FIELD(version) >= 13) {
        const struct dyld_all_image_infos *local_aii = _dyld_get_all_image_infos();
        if (local_aii->version >= 13 &&
            FIELD(sharedCacheSlide) == local_aii->sharedCacheSlide &&
            !memcmp(FIELD(sharedCacheUUID), local_aii->sharedCacheUUID, 16)) {
            return FFI_SHORT_CIRCUIT;
        }
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
        free(info_array);
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
        else if (!strcmp(path_buf, "/usr/lib/system/libsystem_pthread.dylib"))
            *libpthread_p = load_address;

        if (*libdyld_p && *libpthread_p) {
            free(info_array);
            return SUBSTITUTE_OK;
        }

        info_array_ptr += info_array_elm_size;
    }

    free(info_array);
    asprintf(error, "couldn't find libdyld or libpthread");
    return SUBSTITUTE_ERR_MISC;
}

static int get_foreign_image_export(mach_port_t task, uint64_t hdr_addr,
                                    void **linkedit_p, size_t *linkedit_size_p,
                                    void **export_p, size_t *export_size_p,
                                    cpu_type_t *cputype_p, char **error) {
    mach_vm_offset_t hdr_buf = 0;
    mach_vm_size_t hdr_buf_size;
    int ret;

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

    *cputype_p = mh->cputype;

    size_t mh_size = mh->magic == MH_MAGIC_64 ? sizeof(struct mach_header_64)
                                              : sizeof(struct mach_header);
    if (mh->sizeofcmds < mh_size || mh->sizeofcmds > 128*1024)
        goto badmach;

    size_t total_size = mh_size + mh->sizeofcmds;
    if (total_size > hdr_buf_size) {
        vm_deallocate(mach_task_self(), (vm_offset_t) hdr_buf, (vm_size_t) hdr_buf_size);
        hdr_buf_size = total_size;
        hdr_buf = 0;
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
    mach_vm_address_t linkedit_buf = 0;
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

int substitute_dlopen_in_pid(int pid, const char *filename, int options, char **error) {
    mach_port_t task;
    mach_vm_address_t target_stack = 0;
    *error = NULL;
    kern_return_t kr = task_for_pid(mach_task_self(), pid, &task);
    int ret;
    if (kr) {
        asprintf(error, "task_for_pid: kr=%d", kr);
        return SUBSTITUTE_ERR_TASK_FOR_PID;
    }

    uint64_t libdyld_addr, libpthread_addr;
    if ((ret = find_foreign_images(task, &libdyld_addr, &libpthread_addr, error)) > 0)
        goto fail;

    uint64_t dlopen_addr, pthread_create_addr;
    cpu_type_t cputype;
    if (ret == FFI_SHORT_CIRCUIT) {
        dlopen_addr = (uint64_t) dlopen;
        pthread_create_addr = (uint64_t) pthread_create;
#if defined(__x86_64__)
        cputype = CPU_TYPE_X86_64;
#elif defined(__i386__)
        cputype = CPU_TYPE_I386;
#elif defined(__arm__)
        cputype = CPU_TYPE_ARM;
#elif defined(__arm64__)
        cputype = CPU_TYPE_ARM64;
#endif
    } else {
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
                                                &cputype, error)))
                goto fail;
            bool fesr = find_export_symbol(export, export_size, libs[i].symname,
                                           libs[i].addr, &libs[i].symaddr);
            vm_deallocate(mach_task_self(), (vm_offset_t) linkedit, (vm_size_t) linkedit_size);
            if (!fesr) {
                asprintf(error, "couldn't find _dlopen in libdyld");
                ret = SUBSTITUTE_ERR_MISC;
                goto fail;
            }
        }

        dlopen_addr = libs[0].symaddr;
        pthread_create_addr = libs[1].symaddr;
    }

    UNUSED
    extern char inject_page_start[],
                inject_start_x86_64[],
                inject_start_i386[],
                inject_start_arm[],
                inject_start_arm64[];

    int target_page_size = cputype == CPU_TYPE_ARM64 ? 0x4000 : 0x1000;
    kr = mach_vm_allocate(task, &target_stack, 2 * target_page_size, VM_FLAGS_ANYWHERE);
    if (kr) {
        asprintf(error, "couldn't allocate target stack");
        ret = SUBSTITUTE_ERR_OOM;
        goto fail;
    }

    mach_vm_address_t target_code_page = target_stack + target_page_size;
    vm_prot_t cur, max;
    kr = mach_vm_remap(task, &target_code_page, target_page_size, 0,
                       VM_FLAGS_OVERWRITE, mach_task_self(),
                       (mach_vm_address_t) inject_page_start,
                       /*copy*/ false,
                       &cur, &max, VM_INHERIT_NONE);
    if (kr) {
        asprintf(error, "couldn't remap target code");
        ret = SUBSTITUTE_ERR_VM;
        goto fail;
    }

    size_t filelen = strlen(filename);
    if (filelen >= 0x400) {
        asprintf(error, "you gave me a terrible filename (%s)", filename);
        ret = SUBSTITUTE_ERR_MISC;
        goto fail;
    }
    size_t filelen_rounded = (filelen + 7) & ~7;

    size_t baton_len = (cputype & CPU_ARCH_ABI64) ? 32 : 16;
    mach_vm_address_t target_stack_top = target_stack + target_page_size
                                         - baton_len - filelen_rounded;
    char *stackbuf = calloc(baton_len + filelen_rounded, 1);
    if (!stackbuf) {
        ret = SUBSTITUTE_ERR_OOM;
        goto fail;
    }
    strcpy(stackbuf + baton_len, filename);

    uint64_t vals[3] = {pthread_create_addr, dlopen_addr, target_stack_top + baton_len};
    if (cputype & CPU_ARCH_ABI64) {
        uint64_t *p = (void *) stackbuf;
        p[0] = vals[0];
        p[1] = vals[1];
        p[2] = vals[2];
    } else {
        uint32_t *p = (void *) stackbuf;
        p[0] = (uint32_t) vals[0];
        p[1] = (uint32_t) vals[1];
        p[2] = (uint32_t) vals[2];
    }

    kr = mach_vm_write(task, target_stack_top,
                       (mach_vm_address_t) stackbuf, baton_len + filelen_rounded);
    free(stackbuf);
    if (kr) {
        asprintf(error, "mach_vm_write(stack data): kr=%d", kr);
        ret = SUBSTITUTE_ERR_MISC;
        goto fail;
    }

    union {
        struct _x86_thread_state_32 x32;
        struct _x86_thread_state_64 x64;
        struct _arm_thread_state_32 a32;
        struct _arm_thread_state_64 a64;
    } u;
    size_t state_size;
    thread_state_flavor_t flavor;
    memset(&u, 0, sizeof(u));

    switch (cputype) {
#if defined(__x86_64__) || defined(__i386__)
    case CPU_TYPE_X86_64:
        u.x64.rsp = target_stack_top;
        u.x64.rdi = target_stack_top;
        u.x64.rip = target_code_page + (inject_start_x86_64 - inject_page_start);
        state_size = sizeof(u.x64);
        flavor = _x86_thread_state_64_flavor;
        break;
    case CPU_TYPE_I386:
        u.x32.esp = target_stack_top;
        u.x32.ecx = target_stack_top;
        u.x32.eip = target_code_page + (inject_start_i386 - inject_page_start);
        state_size = sizeof(u.x32);
        flavor = _x86_thread_state_32_flavor;
        break;
#endif
#if defined(__arm__) || defined(__arm64__)
    case CPU_TYPE_ARM:
        u.a32.sp = target_stack_top;
        u.a32.r[0] = target_stack_top;
        u.a32.pc = target_code_page + (inject_start_arm - inject_page_start);
        state_size = sizeof(u.a32);
        flavor = _arm_thread_state_32_flavor;
        break;
    case CPU_TYPE_ARM64:
        u.a64.sp = target_stack_top;
        u.a64.x[0] = target_stack_top;
        u.a64.pc = target_code_page + (inject_start_arm64 - inject_page_start);
        state_size = sizeof(u.a64);
        flavor = _arm_thread_state_64_flavor;
        break;
#endif
    default:
        asprintf(error, "unknown target cputype %d", cputype);
        ret = SUBSTITUTE_ERR_MISC;
        goto fail;
    }

    mach_port_t thread;
    kr = thread_create_running(task, flavor, (thread_state_t) &u,
                               state_size / sizeof(int), &thread);
    if (kr) {
        asprintf(error, "thread_create_running: kr=%d", kr);
        ret = SUBSTITUTE_ERR_MISC;
        goto fail;
    }

    target_stack = 0;

    /* it will terminate itself */
    mach_port_deallocate(mach_task_self(), thread);

    (void) options;

    ret = 0;
fail:
    if (target_stack)
        mach_vm_deallocate(task, target_stack, 2 * target_page_size);
    mach_port_deallocate(mach_task_self(), task);
    return ret;
}
#endif /* __APPLE__ */
