#include "substitute.h"
#include "substitute-internal.h"
#include "darwin/mach-decls.h"
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <mach/vm_region.h>

static int unrestrict_macho_header(void *header, size_t size, bool *did_modify_p,
                                   char **error) {
    *did_modify_p = false;
    struct mach_header *mh = header;
    if (mh->magic != MH_MAGIC && mh->magic != MH_MAGIC_64)
        return SUBSTITUTE_ERR_MISC;
    size_t off = mh->magic == MH_MAGIC_64 ? sizeof(struct mach_header_64)
                                          : sizeof(struct mach_header);
    for (uint32_t i = 0; i < mh->ncmds; i++) {
        if (off > size || size - off < sizeof(struct load_command))
            break; /* whatever */
        struct load_command *lc = header + off;
        if (lc->cmdsize > size - off)
            break;
        #define CASES(code...) \
            if (lc->cmd == LC_SEGMENT) { \
                typedef struct segment_command segment_command_y; \
                typedef struct section section_y; \
                code \
            } else if (lc->cmd == LC_SEGMENT_64) { \
                typedef struct segment_command_64 segment_command_y; \
                typedef struct section_64 section_y; \
                code \
            }
        CASES(
            segment_command_y *sc = (void *) lc;
            if (lc->cmdsize < sizeof(*sc) ||
                sc->nsects > (lc->cmdsize - sizeof(*sc)) / sizeof(struct section)) {
                asprintf(error, "bad segment_command");
                return SUBSTITUTE_ERR_MISC;
            }
            if (!strncmp(sc->segname, "__RESTRICT", 16)) {
                section_y *sect = (void *) (sc + 1);
                for (uint32_t i = 0; i < sc->nsects; i++, sect++) {
                    if (!strncmp(sect->sectname, "__restrict", 16)) {
                        strcpy(sect->sectname, "\xf0\x9f\x92\xa9");
                        *did_modify_p = true;
                    }
                }
            }
        )
        #undef CASES

        if (off + lc->cmdsize < off) {
            asprintf(error, "overflowing lc->cmdsize");
            return SUBSTITUTE_ERR_MISC;
        }
        off += lc->cmdsize;
    }
    return SUBSTITUTE_OK;
}

EXPORT
int substitute_ios_unrestrict(task_t task, char **error) {
    *error = NULL;

    int ret;
    vm_address_t header_addr = 0;
    kern_return_t kr;

    /* alrighty then, let's look at the damage.  find the first readable
     * segment */
setback:;
    mach_vm_address_t segm_addr = 0;
    mach_vm_size_t segm_size = 0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object_name;
    while (1) {
        kr = mach_vm_region(task, &segm_addr, &segm_size, VM_REGION_BASIC_INFO_64,
                            (vm_region_info_t) &info, &info_count, &object_name);
        if (kr == KERN_INVALID_ADDRESS) {
            /* nothing! maybe it's not there *yet*? this actually is possible */
            usleep(10);
            goto setback;
        } else if (kr) {
            asprintf(error, "mach_vm_region(%lx): %x", (long) segm_addr, kr);
            ret = SUBSTITUTE_ERR_VM;
            goto fail;
        }
        if (info.protection)
            break;

        segm_addr++;
    }

    size_t toread = 0x4000;
    if (segm_size < toread)
        toread = segm_size;
    if ((kr = vm_allocate(mach_task_self(), &header_addr, toread,
                          VM_FLAGS_ANYWHERE))) {
        asprintf(error, "vm_allocate(%zx): %x", toread, kr);
        ret = SUBSTITUTE_ERR_MISC;
        goto fail;
    }
    mach_vm_size_t actual = toread;
    kr = mach_vm_read_overwrite(task, segm_addr, toread, header_addr,
                                &actual);
    if (kr || actual != toread) {
        asprintf(error, "mach_vm_read_overwrite: %x", kr);
        ret = SUBSTITUTE_ERR_MISC;
        goto fail;
    }

    bool did_modify;
    if ((ret = unrestrict_macho_header((void *) header_addr, toread,
                                       &did_modify, error)))
        goto fail;

    if (did_modify) {
        if ((kr = vm_protect(mach_task_self(), header_addr, toread,
                             FALSE, info.protection))) {
            asprintf(error, "vm_protect(%lx=>%d): %x",
                     (long) header_addr, info.protection, kr);
            ret = SUBSTITUTE_ERR_VM;
            goto fail;
        }
        vm_prot_t cur, max;
        if ((kr = mach_vm_remap(task, &segm_addr, toread, 0,
                                VM_FLAGS_OVERWRITE,
                                mach_task_self(), header_addr, FALSE,
                                &cur, &max, info.inheritance))) {
            asprintf(error, "mach_vm_remap(%lx=>%lx * %zx): %x",
                     (long) header_addr, (long) segm_addr, toread, kr);
            ret = SUBSTITUTE_ERR_VM;
            goto fail;
        }
    }

    ret = SUBSTITUTE_OK;
fail:
    if (header_addr)
        vm_deallocate(mach_task_self(), header_addr, toread);
    return ret;
}


