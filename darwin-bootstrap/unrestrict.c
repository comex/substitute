/* This is an iOS executable spawned from posixspawn-hook.dylib, which accesses
 * a process and changes the name of any __RESTRICT,__restrict sections in the
 * main executable in memory.  Doing so prevents dyld from refusing to honor
 * DYLD_INSERT_LIBRARIES (which is a pretty dumb protection mechanism in the
 * first place, even on OS X).
 *
 * It exists as a separate executable because (a) such processes may be
 * launched with POSIX_SPAWN_SETEXEC, which makes posix_spawn act like exec and
 * replace the current process, and (b) if they're not, launchd/xpcproxy (into
 * which posixspawn-hook is injected) still can't task_for_pid the child
 * process itself, because they don't have the right entitlements.
*/

#define IB_LOG_NAME "unrestrict"
#include "ib-log.h"
#include "darwin/mach-decls.h"
#include "substitute.h"
#include "substitute-internal.h"
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <mach/mach.h>
#include <mach-o/loader.h>

#define PROC_PIDFDVNODEINFO 1
#define PROC_PIDFDVNODEINFO_SIZE 176
int proc_pidfdinfo(int, int, int, void *, int);

static bool unrestrict_macho_header(void *header, size_t size) {
    struct mach_header *mh = header;
    if (mh->magic != MH_MAGIC && mh->magic != MH_MAGIC_64) {
        ib_log("bad mach-o magic");
        return false;
    }
    bool did_modify = false;
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
                ib_log("bad segment_command");
                return false;
            }
            if (!strncmp(sc->segname, "__RESTRICT", 16)) {
                section_y *sect = (void *) (sc + 1);
                for (uint32_t i = 0; i < sc->nsects; i++, sect++) {
                    if (!strncmp(sect->sectname, "__restrict", 16)) {
                        strcpy(sect->sectname, "\xf0\x9f\x92\xa9");
                        did_modify = true;
                    }
                }
            }
        )
        #undef CASES

        if (off + lc->cmdsize < off) {
            ib_log("overflowing lc->cmdsize");
            return false;
        }
        off += lc->cmdsize;
    }
    return did_modify;
}

static void unrestrict(task_t task) {
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
            ib_log("mach_vm_region(%lx): %x", (long) segm_addr, kr);
            return;
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
        ib_log("vm_allocate(%zx): %x", toread, kr);
        return;
    }
    mach_vm_size_t actual = toread;
    kr = mach_vm_read_overwrite(task, segm_addr, toread, header_addr,
                                &actual);
    if (kr || actual != toread) {
        ib_log("mach_vm_read_overwrite: %x", kr);
        return;
    }

    bool did_modify = unrestrict_macho_header((void *) header_addr, toread);

    if (did_modify) {
        if ((kr = vm_protect(mach_task_self(), header_addr, toread,
                             FALSE, info.protection))) {
            ib_log("vm_protect(%lx=>%d): %x",
                   (long) header_addr, info.protection, kr);
            return;
        }
        vm_prot_t cur, max;
        if ((kr = mach_vm_remap(task, &segm_addr, toread, 0,
                                VM_FLAGS_OVERWRITE,
                                mach_task_self(), header_addr, FALSE,
                                &cur, &max, info.inheritance))) {
            ib_log("mach_vm_remap(%lx=>%lx size=%zx): %x",
                   (long) header_addr, (long) segm_addr, toread, kr);
            return;
        }
    }
}



int main(int argc, char **argv) {
    if (argc != 4) {
        ib_log("wrong number of args");
        return 1;
    }

    const char *pids = argv[1];
    char *end;
    long pid = strtol(pids, &end, 10);
    if (!pids[0] || *end) {
        ib_log("pid not an integer");
        return 1;
    }

    const char *should_resume = argv[2];
    if (strcmp(should_resume, "0") && strcmp(should_resume, "1")) {
        ib_log("should_resume not 0 or 1");
        return 1;
    }

    const char *is_exec = argv[3];
    if (strcmp(is_exec, "0") && strcmp(is_exec, "1")) {
        ib_log("is_exec not 0 or 1");
        return 1;
    }

    /* double fork to avoid zombies */
    int ret = fork();
    if (ret == -1) {
        ib_log("fork: %s", strerror(errno));
        return 1;
    } else if (ret) {
        return 0;
    }

    if (IB_VERBOSE) {
        ib_log("unrestricting %ld (sr=%s, ie=%s)", pid,
               should_resume, is_exec);
    }

    int rv = 1;

    int retries = 0;
    int wait_us = 50;
    mach_port_t task;
    while (1) {
        kern_return_t kr = task_for_pid(mach_task_self(), (pid_t) pid, &task);
        if (kr) {
            /* If we're still in the old task (specifically, exec_handle_sugid
             * has not been called yet), task_for_pid will fail because
             * xpcproxy changed uid since it started, and for some dumb reason
             * debugging any such processes is prohibited by
             * task_for_pid_posix_check. */
            if (retries++ == 20) {
                ib_log("TFP still failing (%d) after 20 retries", kr);
                goto fail;
            }
        } else {
            if (is_exec[0] != '1') {
                break;
            } else {
                /* The process might not have transitioned yet.  We set up a
                 * dummy fd 255 in the parent process which was marked CLOEXEC,
                 * so test if that still exists.  AFAICT, Substrate's
                 * equivalent to this is not actually correct.
                 * (I don't think the task_for_pid failure check is sufficient,
                 * as P_SUGID is only set if the credential actually changed.)
                 */
                char buf[PROC_PIDFDVNODEINFO_SIZE];
                /* A bug in proc_pidfdinfo makes it never return -1.  Yuck. */
                errno = 0;
                proc_pidfdinfo(pid, 255, PROC_PIDFDVNODEINFO, buf, sizeof(buf));
                if (errno == EBADF) {
                    break;
                } else if (errno) {
                    ib_log("proc_pidfdinfo: %s", strerror(errno));
                    goto fail;
                }

                if (retries++ == 20) {
                    ib_log("still in parent process after 20 retries");
                    goto fail;
                }
            }
        }
        /* ok, just retry */
        wait_us *= 2;
        if (wait_us > 200000)
            wait_us = 200000;
        while (usleep(wait_us))
            ;
    }

    if (IB_VERBOSE && retries > 0)
        ib_log("note: ready after %d retries", retries);

    unrestrict(task);

    rv = 0;
fail:
    if (should_resume[0] == '1') {
        if ((kill(pid, SIGCONT))) {
            ib_log("kill SIGCONT: %s", strerror(errno));
            return 1;
        }
    }

    return rv;
}
