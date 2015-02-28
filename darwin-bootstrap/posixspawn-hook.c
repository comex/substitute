/* This library is loaded into launchd, and from there into xpcproxy, which
 * launchd (always?) uses as an intermediary to exec its processes; its main
 * purpose is to ensure that bundle-loader.dylib is specified in
 * DYLD_INSERT_LIBRARIES when launching such processes.  In the interests of
 * not making ssh really weird (and because it's what Substrate does), this is
 * separate from bundle-loader itself, so any processes that do their own
 * spawning won't get the environment override.
 *
 * It also handles the sandbox override for substituted. */

#define IB_LOG_NAME "posixspawn-hook"
#include "ib-log.h"
#include "substitute.h"
#include "substitute-internal.h"
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <spawn.h>
#include <sys/wait.h>
#include <syslog.h>
#include <malloc/malloc.h>
#include <assert.h>
#include <errno.h>
#include <arpa/inet.h>
#include <libkern/OSByteOrder.h>

extern char ***_NSGetEnviron(void);

static __typeof__(posix_spawn) *old_posix_spawn, *old_posix_spawnp,
                               hook_posix_spawn, hook_posix_spawnp;
static __typeof__(wait4) *old_wait4, hook_wait4;
static __typeof__(waitpid) *old_waitpid, hook_waitpid;

static bool advance(char **strp, const char *template) {
    size_t len = strlen(template);
    if (!strncmp(*strp, template, len)) {
        *strp += len;
        return true;
    }
    return false;
}

static bool spawn_unrestrict(pid_t pid, bool should_resume, bool is_exec) {
    const char *prog = "/Library/Substitute/unrestrict";
    char pid_s[32];
    sprintf(pid_s, "%ld", (long) pid);
    const char *should_resume_s = should_resume ? "1" : "0";
    const char *is_exec_s = is_exec ? "1" : "0";
    const char *argv[] = {prog, pid_s, should_resume_s, is_exec_s, NULL};
    pid_t prog_pid;
    if (old_posix_spawn(&prog_pid, prog, NULL, NULL, (char **) argv, NULL)) {
        ib_log("posixspawn-hook: couldn't start unrestrict - oh well...");
        return false;
    }
    int xstat;
    /* reap intermediate to avoid zombie - if it doesn't work, not a big deal */
    if (waitpid(prog_pid, &xstat, 0) == -1)
        ib_log("posixspawn-hook: couldn't waitpid");
    ib_log("unrestrict xstat=%x", xstat);
    return true;
}

static bool looks_restricted(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        ib_log("open '%s': %s", filename, strerror(errno));
        return false;
    }
    uint32_t offset = 0;
    union {
        uint32_t magic;
        struct {
            struct fat_header fh;
            struct fat_arch fa1;
        };
        struct mach_header mh;
    } u;
    if (read(fd, &u, sizeof(u)) != sizeof(u)) {
        ib_log("read header for '%s': %s", filename, strerror(errno));
        return false;
    }
    if (ntohl(u.magic) == FAT_MAGIC) {
        /* Fat binary - to avoid needing to replicate grade_binary in the
         * kernel, we assume all architectures have the same restrict-ness. */
         if (u.fh.nfat_arch == 0)
            return false;
        offset = ntohl(u.fa1.offset);
        if (pread(fd, &u, sizeof(u), offset) != sizeof(u)) {
            ib_log("read header (inside fat) for '%s': %s",
                   filename, strerror(errno));
            return false;
        }
    }
    bool swap, is64;
    switch (u.magic) {
    case MH_MAGIC:
        swap = false;
        is64 = false;
        break;
    case MH_MAGIC_64:
        swap = false;
        is64 = true;
        break;
    case MH_CIGAM:
        swap = true;
        is64 = false;
        break;
    case MH_CIGAM_64:
        swap = true;
        is64 = true;
        break;
    default:
        ib_log("bad mach-o magic for '%s'", filename);
        return false;
    }
    uint32_t sizeofcmds = u.mh.sizeofcmds;
    if (swap)
        sizeofcmds = OSSwapInt32(sizeofcmds);
    offset += is64 ? sizeof(struct mach_header_64) : sizeof(struct mach_header);
    char *cmds_buf = malloc(sizeofcmds);
    ssize_t actual = pread(fd, cmds_buf, sizeofcmds, offset);
    if (actual < 0 || (uint32_t) actual != sizeofcmds) {
        ib_log("read load cmds for '%s': %s", filename, strerror(errno));
        free(cmds_buf);
        return false;
    }
    /* overestimation is fine here */
    const char sectname[] = "__restrict";
    bool ret = !!memmem(cmds_buf, sizeofcmds, sectname, sizeof(sectname));
    free(cmds_buf);
    return ret;
}

static int hook_posix_spawn_generic(__typeof__(posix_spawn) *old,
                                    pid_t *restrict pidp, const char *restrict path,
                                    const posix_spawn_file_actions_t *file_actions,
                                    const posix_spawnattr_t *restrict attrp,
                                    char *const argv[restrict],
                                    char *const envp[restrict]) {
    char *new = NULL;
    char **new_envp = NULL;
    char *const *envp_to_use = envp;
    char *const *my_envp = envp ? envp : *_NSGetEnviron();
    posix_spawnattr_t my_attr = NULL;

    if (attrp) {
        posix_spawnattr_t attr = *attrp;
        size_t size = malloc_size(attr);
        my_attr = malloc(size);
        if (!my_attr)
            goto crap;
        memcpy(my_attr, attr, size);
    } else {
        if (posix_spawnattr_init(&my_attr))
            goto crap;
    }

    short flags;
    if (posix_spawnattr_getflags(&my_attr, &flags))
        goto crap;
    if (IB_VERBOSE) {
        ib_log("hook_posix_spawn_generic: path=%s%s%s",
               path,
               (flags & POSIX_SPAWN_SETEXEC) ? " (exec)" : "",
               (flags & POSIX_SPAWN_START_SUSPENDED) ? " (suspend)" : "");
        for (char *const *ap = argv; *ap; ap++)
            ib_log("   %s", *ap);
    }

    /* This mirrors Substrate's logic with safe mode.  I don't really
     * understand the point of the difference between its 'safe' (which removes
     * Substrate from DYLD_INSERT_LIBRARIES) and 'quit' (which just skips
     * directly to the original spawn), but I guess I'll just do the same for
     * maximum safety... */
    bool safe_mode = false;

    const char *orig_dyld_insert = "";
    static const char bl_dylib[] =
        "/Library/Substitute/bundle-loader.dylib";
    static const char psh_dylib[] =
        "/Library/Substitute/posixspawn-hook.dylib";
    size_t env_count = 0;
    for (char *const *ep = my_envp; *ep; ep++) {
        env_count++;
        char *env = *ep;
        if (advance(&env, "_MSSafeMode=") || advance(&env, "_SubstituteSafeMode=")) {
            if (!strcmp(env, "0") || !strcmp(env, "NO"))
                continue;
            else if (!strcmp(env, "1") || !strcmp(env, "YES"))
                safe_mode = true;
            else
                goto skip;
        } else if (advance(&env, "DYLD_INSERT_LIBRARIES=")) {
            orig_dyld_insert = env;
        }
    }
    new = malloc(sizeof("DYLD_INSERT_LIBRARIES=") - 1 +
                 sizeof(psh_dylib) /* not - 1, because : */ +
                 strlen(orig_dyld_insert) + 1);
    char *newp_orig = stpcpy(new, "DYLD_INSERT_LIBRARIES=");
    char *newp = newp_orig;
    const char *p = orig_dyld_insert;
    while (*p) { /* W.N.H. */
        const char *next = strchr(p, ':') ?: (p + strlen(p));
        /* append if it isn't one of ours */
        bool is_substitute =
            (next - p == sizeof(bl_dylib) - 1 &&
             !memcmp(next, bl_dylib, sizeof(bl_dylib) - 1)) ||
            (next - p == sizeof(psh_dylib) - 1 &&
             !memcmp(next, psh_dylib, sizeof(psh_dylib) - 1));
        if (!is_substitute) {
            if (newp != newp_orig)
                *newp++ = ':';
            memcpy(newp, p, next - p);
            newp += next - p;
        }
        if (!*next)
            break;
        p = next + 1;
    }
    /* append ours if necessary */
    if (!safe_mode) {
        if (newp != newp_orig)
            *newp++ = ':';
        const char *dylib_to_add = !strcmp(path, "/usr/libexec/xpcproxy")
                                   ? psh_dylib
                                   : bl_dylib;
        newp = stpcpy(newp, dylib_to_add);
    }
    if (IB_VERBOSE)
        ib_log("using %s", new);
    /* no libraries? then just get rid of it */
    if (newp == newp_orig) {
        free(new);
        new = NULL;
    }
    new_envp = malloc(sizeof(char *) * (env_count + 2));
    envp_to_use = new_envp;
    char **outp = new_envp;
    for (size_t idx = 0; idx < env_count; idx++) {
        char *env = my_envp[idx];
        /* remove *all* D_I_L, including duplicates */
        if (!advance(&env, "DYLD_INSERT_LIBRARIES="))
            *outp++ = env;
    }
    if (new)
        *outp++ = new;
    *outp++ = NULL;

    if (safe_mode)
        goto skip;


    /* Deal with the dumb __restrict section.  A complication is that this
     * could actually be an exec. */
    bool need_unrestrict = looks_restricted(path);

    /* TODO skip this if Substrate is doing it anyway */
    bool was_suspended;
    if (need_unrestrict) {
        was_suspended = flags & POSIX_SPAWN_START_SUSPENDED;
        flags |= POSIX_SPAWN_START_SUSPENDED;
        if (posix_spawnattr_setflags(&my_attr, flags))
            goto crap;
        if (flags & POSIX_SPAWN_SETEXEC) {
            /* make the marker fd; hope you weren't using that */
            if (dup2(2, 255) != 255) {
                ib_log("dup2 failure - %s", strerror(errno));
                goto skip;
            }
            if (fcntl(255, F_SETFD, FD_CLOEXEC))
                goto crap;
            if (IB_VERBOSE)
                ib_log("spawning unrestrict");
            if (!spawn_unrestrict(getpid(), !was_suspended, true))
                goto skip;
        }
    }
    if (IB_VERBOSE)
        ib_log("**");
    int ret = old(pidp, path, file_actions, &my_attr, argv, envp_to_use);
    if (IB_VERBOSE)
        ib_log("ret=%d pid=%ld", ret, (long) *pidp);
    if (ret)
        goto cleanup;
    /* Since it returned, obviously it was not SETEXEC, so we need to
     * unrestrict it ourself. */
    pid_t pid = *pidp;
    if (need_unrestrict)
        spawn_unrestrict(pid, !was_suspended, false);
    goto cleanup;
crap:
    ib_log("posixspawn-hook: weird error - OOM?  skipping our stuff");
skip:
    ret = old(pidp, path, file_actions, attrp, argv, envp);
cleanup:
    free(new_envp);
    free(new);
    free(my_attr);
    return ret;
}

static void after_wait_generic(pid_t pid, int stat) {
    /* TODO safety */
    (void) pid;
    (void) stat;
}

int hook_posix_spawn(pid_t *restrict pid, const char *restrict path,
                     const posix_spawn_file_actions_t *file_actions,
                     const posix_spawnattr_t *restrict attrp,
                     char *const argv[restrict], char *const envp[restrict]) {
    return hook_posix_spawn_generic(old_posix_spawn, pid, path, file_actions,
                                    attrp, argv, envp);
}

int hook_posix_spawnp(pid_t *restrict pid, const char *restrict path,
                      const posix_spawn_file_actions_t *file_actions,
                      const posix_spawnattr_t *restrict attrp,
                      char *const argv[restrict], char *const envp[restrict]) {
    return hook_posix_spawn_generic(old_posix_spawnp, pid, path, file_actions,
                                    attrp, argv, envp);
}

pid_t hook_wait4(pid_t pid, int *stat_loc, int options, struct rusage *rusage) {
    pid_t ret = old_wait4(pid, stat_loc, options, rusage);
    after_wait_generic(ret, *stat_loc);
    return ret;
}

pid_t hook_waitpid(pid_t pid, int *stat_loc, int options) {
    pid_t ret = old_waitpid(pid, stat_loc, options);
    after_wait_generic(ret, *stat_loc);
    return ret;
}

void substitute_init(struct shuttle *shuttle, size_t nshuttle) {
    /* Just tell them we're done */
    if (nshuttle != 1) {
        ib_log("nshuttle = %zd?", nshuttle);
        return;
    }
    mach_port_t notify_port = shuttle[0].u.mach.port;
    mach_msg_header_t done_hdr;
    done_hdr.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND, 0);
    done_hdr.msgh_size = sizeof(done_hdr);
    done_hdr.msgh_remote_port = notify_port;
    done_hdr.msgh_local_port = 0;
    done_hdr.msgh_voucher_port = 0;
    done_hdr.msgh_id = 42;
    kern_return_t kr = mach_msg_send(&done_hdr);
    if (kr)
        ib_log("posixspawn-hook: mach_msg_send failed: kr=%x", kr);
    /* MOVE deallocated the port */
}

__attribute__((constructor))
static void init() {
    /* Note: I'm using interposing to minimize the chance of conflict with
     * Substrate.  This shouldn't actually be necessary, because MSHookProcess,
     * at least as of the old version I'm looking at the source code of, blocks
     * until the thread it remotely creates exits, and that thread does
     * pthread_join on the 'real' pthread it creates to do the dlopen (unlike
     * the equivalent in Substitute - the difference is to decrease dependence
     * on pthread internals).  substitute_dlopen_in_pid does not, but that's
     * what the notify port is for.  Meanwhile, the jailbreak I have installed
     * properly runs rc.d sequentially, so the injection tools won't do their
     * thing at the same time.  But just in case any of that doesn't hold up...
     *
     * (it also decreases the amount of library code necessary to load from
     * disk...)
     */

    struct substitute_image *im = substitute_open_image(_dyld_get_image_name(0));
    if (!im) {
        ib_log("posixspawn-hook: substitute_open_image failed");
        goto end;
    }

    static const struct substitute_import_hook hooks[] = {
        {"_posix_spawn", hook_posix_spawn, &old_posix_spawn},
        {"_posix_spawnp", hook_posix_spawnp, &old_posix_spawnp},
        {"_waitpid", hook_waitpid, &old_waitpid},
        {"_wait4", hook_wait4, &old_wait4},
    };

    int err = substitute_interpose_imports(im, hooks, sizeof(hooks)/sizeof(*hooks), 0);
    if (err) {
        ib_log("posixspawn-hook: substitute_interpose_imports failed: %s",
               substitute_strerror(err));
        goto end;
    }

end:
    if (im)
        substitute_close_image(im);

}
