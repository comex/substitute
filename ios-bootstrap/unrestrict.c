#define IB_LOG_NAME "unrestrict"
#include "ib-log.h"
#include "substitute.h"
#include "substitute-internal.h"
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>

#define PROC_PIDFDVNODEINFO 1
#define PROC_PIDFDVNODEINFO_SIZE 176
int proc_pidfdinfo(int, int, int, void *, int);

int main(int argc, char **argv) {
    if (argc != 4) {
        ib_log("unrestrict: wrong number of args");
        return 1;
    }

    const char *pids = argv[1];
    char *end;
    long pid = strtol(pids, &end, 10);
    if (!pids[0] || *end) {
        ib_log("unrestrict: pid not an integer");
        return 1;
    }

    const char *should_resume = argv[2];
    if (strcmp(should_resume, "0") && strcmp(should_resume, "1")) {
        ib_log("unrestrict: should_resume not 0 or 1");
        return 1;
    }

    const char *is_exec = argv[3];
    if (strcmp(is_exec, "0") && strcmp(is_exec, "1")) {
        ib_log("unrestrict: is_exec not 0 or 1");
        return 1;
    }

    /* double fork to avoid zombies */
    int ret = fork();
    if (ret == -1) {
        ib_log("unrestrict: fork: %s", strerror(errno));
        return 1;
    } else if (ret) {
        return 0;
    }

    if (IB_VERBOSE) {
        ib_log("unrestrict: unrestricting %ld (sr=%s, ie=%s)", pid,
               should_resume, is_exec);
    }

    int ec = 1;

    mach_port_t task;
    kern_return_t kr = task_for_pid(mach_task_self(), (pid_t) pid, &task);
    if (kr) {
        ib_log("unrestrict: TFP fail: %d", kr);
        goto fail;
    }

    if (is_exec[0] == '1') {
        int retries = 0;
        int wait_us = 1;
        while (1) {
            /* The process might not have transitioned yet.  We set up a dummy fd
             * 255 in the parent process which was marked CLOEXEC, so test if that
             * still exists.  AFAICT, Substrate's equivalent to this is not
             * actually correct.
             * TODO cleanup
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
            wait_us *= 2;
            if (wait_us > 200000)
                wait_us = 200000;
            while (usleep(wait_us))
                ;
        }
    }

    char *err = NULL;
    int sret = substitute_ios_unrestrict(task, &err);
    if (sret) {
        ib_log("unrestrict: substitute_ios_unrestrict => %d (%s)",
               sret, err);
        ec = 1;
    }

    ec = 0;
fail:
    if (should_resume[0] == '1') {
        if ((kill(pid, SIGCONT))) {
            ib_log("unrestrict: kill SIGCONT: %s", strerror(errno));
            return 1;
        }
    }

    return ec;
}
