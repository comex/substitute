#include "substitute.h"
#include "substitute-internal.h"
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <spawn.h>
#include <sys/wait.h>
#include <syslog.h>
#include <malloc/malloc.h>
#include <assert.h>

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

static int hook_posix_spawn_generic(__typeof__(posix_spawn) *old,
                                    pid_t *restrict pid, const char *restrict path,
                                    const posix_spawn_file_actions_t *file_actions,
                                    const posix_spawnattr_t *restrict attrp,
                                    char *const argv[restrict],
                                    char *const envp[restrict]) {
    char *new = NULL;
    char **new_envp = NULL;
    char *const *envp_to_use = envp;
    char *const *my_envp = envp ? envp : *_NSGetEnviron();
    posix_spawnattr_t my_attr = NULL;
    /* This mirrors Substrate's logic with safe mode.  I don't really
     * understand the point of the difference between its 'safe' (which removes
     * Substrate from DYLD_INSERT_LIBRARIES) and 'quit' (which just skips
     * directly to the original spawn), but I guess I'll just do the same for
     * maximum safety... */
    bool safe_mode = false;
    const char *orig_dyld_insert = "";
    static const char my_dylib[] =
        "/Library/Substitute/generic-dyld-inserted.dylib";
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
                 sizeof(my_dylib) - 1 +
                 strlen(orig_dyld_insert) + 1);
    char *newp_orig = stpcpy(new, "DYLD_INSERT_LIBRARIES=");
    char *newp = newp_orig;
    const char *p = orig_dyld_insert;
    while (*p) { /* W.N.H. */
        const char *next = strchr(p, ':') ?: (p + strlen(p));
        /* append if it isn't a copy of ours */
        if (!(next - p == sizeof(my_dylib) - 1 &&
              memcmp(next, my_dylib, sizeof(my_dylib) - 1))) {
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
        newp = stpcpy(newp, my_dylib);
    }
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
    bool was_suspended = flags & POSIX_SPAWN_START_SUSPENDED;
    flags |= POSIX_SPAWN_START_SUSPENDED;
    if (posix_spawnattr_setflags(&my_attr, flags))
        goto crap;
    if (flags & POSIX_SPAWN_SETEXEC) {
        const char *prog = "/Library/Substitute/unrestrict-me";
        char pid[32];
        sprintf(pid, "%ld", (long) getpid());
        const char *should_resume = was_suspended ? "0" : "1";
        const char *argv[] = {prog, pid, should_resume, NULL};
        pid_t prog_pid;
        if (old_posix_spawn(&prog_pid, prog, NULL, NULL, (char **) argv, envp)) {
            syslog(LOG_EMERG,
                   "posixspawn-hook: couldn't start unrestrict-me - oh well...");
            goto skip;
        }
        int xstat;
        /* reap intermediate to avoid zombie - if it doesn't work, not a big deal */
        if (waitpid(prog_pid, &xstat, 0))
            syslog(LOG_ERR, "posixspawn-hook: couldn't waitpid");
    }

    int ret = old(pid, path, file_actions, &my_attr, argv, envp_to_use);
    if (ret)
        goto cleanup;
    /* Since it returned, obviously it was not SETEXEC, so we need to
     * unrestrict ourself. */
    char *error;
    int sret = substitute_ios_unrestrict(*pid, !was_suspended, &error);
    if (sret) {
        syslog(LOG_EMERG, "posixspawn-hook: substitute_ios_unrestrict => %d (%s)",
               sret, error);
    }
    free(error);
    goto cleanup;
crap:
    syslog(LOG_EMERG, "posixspawn-hook: weird error - OOM?  skipping our stuff");
skip:
    ret = old(pid, path, file_actions, attrp, argv, envp);
cleanup:
    free(new_envp);
    free(new);
    free(my_attr);
    return ret;
}

static void after_wait_generic(pid_t pid, int stat) {
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

void substitute_init(struct shuttle *shuttle, UNUSED size_t nshuttle) {
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
        syslog(LOG_EMERG, "posixspawn-hook: substitute_open_image failed");
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
        syslog(LOG_EMERG, "posixspawn-hook: substitute_interpose_imports failed: %s",
               substitute_strerror(err));
        goto end;
    }

end:
    if (im)
        substitute_close_image(im);

    mach_port_t notify_port = shuttle[0].u.mach.port;
    mach_msg_header_t done_hdr;
    done_hdr.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND, 0);
    done_hdr.msgh_size = sizeof(done_hdr);
    done_hdr.msgh_remote_port = notify_port;
    done_hdr.msgh_local_port = 0;
    done_hdr.msgh_voucher_port = 0;
    done_hdr.msgh_id = 42;
    if (mach_msg_send(&done_hdr)) /* MOVE deallocates port */
        syslog(LOG_EMERG, "posixspawn-hook: mach_msg_send failed");
}

__attribute__((constructor))
static void init() {
    if (getenv("TEST_POSIXSPAWN_HOOK")) {
        mach_port_t port;
        assert(!mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_DEAD_NAME,
                                   &port));
        struct shuttle shuttle = {
            .type = SUBSTITUTE_SHUTTLE_MACH_PORT,
            .u.mach.port = port
        };
        substitute_init(&shuttle, 1);
    }
}
