/* This library is loaded into launchd, and from there into xpcproxy, which
 * launchd uses as an intermediary to exec its processes; its main purpose is
 * to ensure that bundle-loader.dylib is specified in DYLD_INSERT_LIBRARIES
 * when launching such processes.  In the interests of not making ssh really
 * weird (and because it's what Substrate does), this is separate from
 * bundle-loader itself, so any processes that do their own spawning won't get
 * the environment override (though D_I_L will be inherited if the environment
 * isn't reset).
 *
 * It also handles the sandbox override for substituted.
 *
 * Note: Because bundle-loader synchronously contacts substituted, it must not
 * be loaded into any synchronous stuff launchd runs before starting jobs
 * proper.  Therefore, it's only inserted if the spawning process is xpcproxy
 * (rather than launchd directly).  I don't think iOS 7 does this yet, so this
 * needs to be fixed there.
 */

#define IB_LOG_NAME "posixspawn-hook"
#include "ib-log.h"
#include "substitute.h"
#include "substitute-internal.h"
#include "darwin/xxpc.h"
#include "cbit/htab.h"
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <spawn.h>
#include <sys/wait.h>
#include <syslog.h>
#include <malloc/malloc.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <libkern/OSByteOrder.h>

#define _pid_hash(pidp) (*(pidp))
#define _pid_eq(pid1p, pid2p) (*(pid1p) == *(pid2p))
#define _pid_null(pidp) (!*(pidp))
DECL_STATIC_HTAB_KEY(pid_t, pid_t, _pid_hash, _pid_eq, _pid_null, 0);
DECL_HTAB(pid_str, pid_t, char *);

extern char ***_NSGetEnviron(void);

struct au_tid;
extern void audit_token_to_au32(audit_token_t, uid_t *, uid_t *, gid_t *,
                                uid_t *, gid_t *, pid_t *, pid_t *,
                                struct au_tid *);

static typeof(posix_spawn) *old_posix_spawn, *old_posix_spawnp,
                           hook_posix_spawn, hook_posix_spawnp;
static typeof(wait4) *old_wait4, hook_wait4;
static typeof(waitpid) *old_waitpid, hook_waitpid;
static int (*old_sandbox_check)(pid_t, const char *, int type, ...);
static int (*old_xpc_pipe_try_receive)(mach_port_t, xxpc_object_t *,
                                       mach_port_t *, void *, size_t, int);

static bool g_is_launchd;
static pthread_mutex_t g_state_lock = PTHREAD_MUTEX_INITIALIZER;
/* covering... */
static xxpc_object_t g_bundleid_to_fate;
static HTAB_STORAGE(pid_str) g_pid_to_bundleid =
    HTAB_STORAGE_INIT_STATIC(&g_pid_to_bundleid, pid_str);

static bool advance(char **strp, const char *template) {
    size_t len = strlen(template);
    if (!strncmp(*strp, template, len)) {
        *strp += len;
        return true;
    }
    return false;
}

static bool spawn_unrestrict(pid_t pid, bool should_resume, bool is_exec) {
    const char *prog = "/Library/Substitute/Helpers/unrestrict";
    char pid_s[32];
    sprintf(pid_s, "%ld", (long) pid);
    const char *should_resume_s = should_resume ? "1" : "0";
    const char *is_exec_s = is_exec ? "1" : "0";
    const char *argv[] = {prog, pid_s, should_resume_s, is_exec_s, NULL};
    pid_t prog_pid;
    char *env[] = {"_MSSafeMode=1", NULL};
    if (old_posix_spawn(&prog_pid, prog, NULL, NULL, (char **) argv, env)) {
        ib_log("posixspawn-hook: couldn't start unrestrict - oh well...");
        return false;
    }
    if (IB_VERBOSE)
        ib_log("unrestrict pid: %d; should_resume=%d is_exec=%d",
               prog_pid, should_resume, is_exec);
    int xstat;
    /* reap intermediate to avoid zombie - if it doesn't work, not a big deal */
    if (waitpid(prog_pid, &xstat, 0) == -1)
        ib_log("posixspawn-hook: couldn't waitpid");
    if (IB_VERBOSE)
        ib_log("unrestrict xstat=%x", xstat);
    return true;
}

static bool looks_restricted(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        ib_log("open '%s': %s", filename, strerror(errno));
        return false;
    }
    bool ret = false;
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
        goto end;
    }
    if (ntohl(u.magic) == FAT_MAGIC) {
        /* Fat binary - to avoid needing to replicate grade_binary in the
         * kernel, we assume all architectures have the same restrict-ness. */
         if (u.fh.nfat_arch == 0)
            goto end;
        offset = ntohl(u.fa1.offset);
        if (pread(fd, &u, sizeof(u), offset) != sizeof(u)) {
            ib_log("read header (inside fat) for '%s': %s",
                   filename, strerror(errno));
            goto end;
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
        goto end;
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
        goto end;
    }
    /* overestimation is fine here */
    const char sectname[] = "__restrict";
    ret = !!memmem(cmds_buf, sizeofcmds, sectname, sizeof(sectname));
    free(cmds_buf);
end:
    close(fd);
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

    short flags = 0;
    if (attrp && posix_spawnattr_getflags(attrp, &flags))
        goto crap;

    if (IB_VERBOSE) {
        ib_log("hook_posix_spawn_generic: path=%s%s%s (ld=%d)",
               path,
               (flags & POSIX_SPAWN_SETEXEC) ? " (exec)" : "",
               (flags & POSIX_SPAWN_START_SUSPENDED) ? " (suspend)" : "",
               g_is_launchd);
        for (char *const *ap = argv; *ap; ap++)
            ib_log("   %s", *ap);
    }


    static const char bl_dylib[] =
        "/Library/Substitute/Helpers/bundle-loader.dylib";
    static const char psh_dylib[] =
        "/Library/Substitute/Helpers/posixspawn-hook.dylib";

    const char *bundleid = NULL;

    /* which dylib should we add, if any? */
    const char *dylib_to_add;
    if (g_is_launchd) {
        if (strcmp(path, "/usr/libexec/xpcproxy"))
            goto skip;
        if (argv[0] && argv[1])
            bundleid = argv[1];
        dylib_to_add = psh_dylib;
    } else {
        /* - substituted obviously doesn't want to have bundle_loader run in it
         *   and try to contact substituted.  I have _MSSafeMode=1 in the plist
         *   so that Substrate also leaves it alone, and that's also checked in
         *   this routine, so the strcmp is just a backup.
         * - I am not sure why notifyd is an issue.  Some libc functions
         *   (localtime) synchronously contact it, which launchd could be
         *   calling, but I haven't caught it in the act.  XXX I'd like to be
         *   completely sure that notifyd and nothing else is a problem.
         * - sshd is here because one of its routines tries to close all file
         *   descriptors after a certain number - fine in a sane system, but
         *   here, if a file descriptor opened with guarded_open_np is closed
         *   with close, it crashes the process (and I don't see any way to
         *   cheat and disable the guard without actually knowing it).
         *   bundle-loader uses xpc, which uses dispatch, which uses
         *   guarded_open_np for its descriptors.  I could try to hook
         *   guarded_open_np for dispatch instead, but that doesn't help if an
         *   actual loaded bundle uses it from some other library, and I don't
         *   want to completely disable this bug detection measure for all
         *   processes.  Just excluding it from hooking is easier, and doing so
         *   provides a tiny bit of extra safety anyway, because ssh can
         *   sometimes be used as a last resort if hooking is screwing
         *   something up.
         * note: sshd is started with the wrapper, with argv[0] != path
         */
        if (!strcmp(path, "/Library/Substitute/Helpers/substituted") ||
            !strcmp(path, "/usr/sbin/notifyd") ||
            !strcmp(xbasename(argv[0] ?: ""), "sshd"))
            goto skip;
        dylib_to_add = bl_dylib;
    }

    if (access(dylib_to_add, R_OK)) {
        /* Substitute must have been uninstalled or something.  In the future
         * we'll be able to actually unload from launchd, but this is still
         * useful as a safety measure. */
        goto skip;
    }

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
    /* This mirrors Substrate's logic with safe mode.  I don't really
     * understand the point of the difference between its 'safe' (which removes
     * Substrate from DYLD_INSERT_LIBRARIES) and 'quit' (which just skips
     * directly to the original spawn), but I guess I'll just do the same for
     * maximum safety... */
    bool safe_mode = false;
    const char *orig_dyld_insert = "";
    size_t env_count = 0;
    for (char *const *ep = my_envp; *ep; ep++) {
        env_count++;
        char *env = *ep;
        if (advance(&env, "_MSSafeMode=") ||
            advance(&env, "_SubstituteSafeMode=")) {
            if (IB_VERBOSE)
                ib_log("got safe mode env: %s", *ep);
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
             !memcmp(p, bl_dylib, sizeof(bl_dylib) - 1)) ||
            (next - p == sizeof(psh_dylib) - 1 &&
             !memcmp(p, psh_dylib, sizeof(psh_dylib) - 1));
        if (!is_substitute) {
            if (newp != newp_orig)
                *newp++ = ':';
            memcpy(newp, p, next - p);
            newp += next - p;
            *newp = '\0';
        }
        if (!*next)
            break;
        p = next + 1;
    }
    /* append ours if necessary */
    if (!safe_mode) {
        if (newp != newp_orig)
            *newp++ = ':';
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

    if (bundleid) {
        pthread_mutex_lock(&g_state_lock);
        bool new_entry;
        char **old_bundleid = htab_setp_pid_str(&g_pid_to_bundleid.h, &pid, &new_entry);
        if (!new_entry)
            free(*old_bundleid);
        *old_bundleid = strdup(bundleid);
        pthread_mutex_unlock(&g_state_lock);
    }

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

static int hook_posix_spawn(pid_t *restrict pid, const char *restrict path,
                            const posix_spawn_file_actions_t *file_actions,
                            const posix_spawnattr_t *restrict attrp,
                            char *const argv[restrict],
                            char *const envp[restrict]) {
    return hook_posix_spawn_generic(old_posix_spawn, pid, path, file_actions,
                                    attrp, argv, envp);
}

static int hook_posix_spawnp(pid_t *restrict pid, const char *restrict path,
                             const posix_spawn_file_actions_t *file_actions,
                             const posix_spawnattr_t *restrict attrp,
                             char *const argv[restrict],
                             char *const envp[restrict]) {
    return hook_posix_spawn_generic(old_posix_spawnp, pid, path, file_actions,
                                    attrp, argv, envp);
}

static void after_wait_generic(pid_t pid, int stat) {
    if (pid == -1)
        return;
    pthread_mutex_lock(&g_state_lock);
    struct htab_bucket_pid_str *bucket =
        htab_getbucket_pid_str(&g_pid_to_bundleid.h, &pid);
    if (!bucket) {
        /* probably spawned some other way / not a task */
        if (IB_VERBOSE)
            ib_log("reaped unknown pid %d", pid);
        goto end;
    }
    char *bundleid = bucket->value;
    xxpc_dictionary_set_int64(g_bundleid_to_fate, bundleid, stat);
    free(bundleid);
    htab_removeat_pid_str(&g_pid_to_bundleid.h, bucket);
end:
    pthread_mutex_unlock(&g_state_lock);
}

static pid_t hook_wait4(pid_t pid, int *stat_loc, int options,
                        struct rusage *rusage) {
    pid_t ret = old_wait4(pid, stat_loc, options, rusage);
    after_wait_generic(ret, *stat_loc);
    return ret;
}

static pid_t hook_waitpid(pid_t pid, int *stat_loc, int options) {
    pid_t ret = old_waitpid(pid, stat_loc, options);
    after_wait_generic(ret, *stat_loc);
    return ret;
}

static int hook_xpc_pipe_try_receive(mach_port_t port_set, xxpc_object_t *requestp,
                                     mach_port_t *recvp, void *mig_demux,
                                     size_t msg_size, int dunno_ignored) {
    int res = old_xpc_pipe_try_receive(port_set, requestp, recvp, mig_demux,
                                       msg_size, dunno_ignored);
    if (res)
        return res;
    xxpc_object_t request = *requestp;
    if (!request || /* just to be sure */
        xxpc_get_type(request) != XXPC_TYPE_DICTIONARY)
        return res;
    /* is it for us? - usage of "in"/"out" is to satisfy the public vproc API */
    xxpc_object_t in = xxpc_dictionary_get_value(request, "in");
    if (!in || xxpc_get_type(in) != XXPC_TYPE_DICTIONARY)
        return res;
    const char *name = xxpc_dictionary_get_string(in,
        "com.ex.substitute.hook-operation");
    if (!name)
        return res;
    /* is it from someone untrustworthy? */
    audit_token_t at;
    xxpc_dictionary_get_audit_token(request, &at);
    uid_t euid;
    pid_t pid;
    audit_token_to_au32(at, NULL, &euid, NULL, NULL, NULL, &pid, NULL, NULL);
    if (euid != 0) {
        ib_log("Attempt to perform hook-operation by pid %d with euid %d",
               euid, pid);
        return res;
    }
    xxpc_object_t reply = NULL;
    if (!strcmp(name, "bundleid-to-fate")) {
        const char *bundleid = xxpc_dictionary_get_string(in, "bundleid");
        if (!bundleid)
            goto invalid;
        reply = xxpc_dictionary_create_reply(request);
        xxpc_object_t out = xxpc_dictionary_create(NULL, NULL, 0);
        pthread_mutex_lock(&g_state_lock);
        xxpc_object_t fate = xxpc_dictionary_get_value(g_bundleid_to_fate,
                                                       bundleid);
        if (fate) {
            if (IB_VERBOSE) {
                char *desc = xxpc_copy_description(fate);
                ib_log("your (%s) fate is %s", bundleid, desc);
                free(desc);
            }
            xxpc_dictionary_set_value(out, "fate", fate);
        } else {
            if (IB_VERBOSE)
                ib_log("your (%s) fate is unavailable", bundleid);
        }
        pthread_mutex_unlock(&g_state_lock);
        xxpc_dictionary_set_value(reply, "out", out);
        xxpc_release(out);
    } else {
        ib_log("unknown hook-operation '%s'", name);
        return res;
    }

    if (reply) {
        int reply_res = xxpc_pipe_routine_reply(reply);
        if (reply_res) {
            ib_log("xxpc_pipe_routine_reply: %d", reply_res);
            return res;
        }
        xxpc_release(reply);
    }
    xxpc_release(request);
    *requestp = NULL;
    return 0;

invalid:
    ib_log("invalid hook-operation='%s' message", name);
    return res;
}

static int hook_sandbox_check(pid_t pid, const char *op, int type, ...) {
    /* Can't easily determine the number of arguments, so just assume there's
     * less than 5 pointers' worth. */
    va_list ap;
    va_start(ap, type);
    long blah[5];
    for (int i = 0; i < 5; i++)
        blah[i] = va_arg(ap, long);
    va_end(ap);
    if (!strcmp(op, "mach-lookup")) {
        const char *name = (void *) blah[0];
        if (!strcmp(name, "com.ex.substituted")) {
            /* always allow */
            return 0;
        }
    }
    return old_sandbox_check(pid, op, type,
                             blah[0], blah[1], blah[2], blah[3], blah[4]);
}

EXPORT
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
    g_bundleid_to_fate = xxpc_dictionary_create(NULL, NULL, 0);

    const char *image0 = _dyld_get_image_name(0);
    g_is_launchd = !!strstr(image0, "launchd");
    struct substitute_image *im = substitute_open_image(image0);
    if (!im) {
        ib_log("posixspawn-hook: substitute_open_image failed");
        goto end;
    }

    static const struct substitute_import_hook hooks[] = {
        {"_posix_spawn", hook_posix_spawn, &old_posix_spawn},
        {"_posix_spawnp", hook_posix_spawnp, &old_posix_spawnp},
        {"_sandbox_check", hook_sandbox_check, &old_sandbox_check},
        {"_waitpid", hook_waitpid, &old_waitpid},
        {"_wait4", hook_wait4, &old_wait4},
        {"_xpc_pipe_try_receive", hook_xpc_pipe_try_receive,
         &old_xpc_pipe_try_receive},
    };

    int err = substitute_interpose_imports(im, hooks, sizeof(hooks)/sizeof(*hooks),
                                           NULL, 0);
    if (err) {
        ib_log("posixspawn-hook: substitute_interpose_imports failed: %s",
               substitute_strerror(err));
        goto end;
    }

end:
    if (im)
        substitute_close_image(im);
}
