#include "substitute.h"
#include "substitute-internal.h"
#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        syslog(LOG_EMERG, "unrestrict: wrong number of args");
        return 1;
    }
    const char *pids = argv[1];
    char *end;
    long pid = strtol(pids, &end, 10);
    if (!pids[0] || *end) {
        syslog(LOG_EMERG, "unrestrict: pid not an integer");
        return 1;
    }

    const char *should_resume = argv[2];
    if (strcmp(should_resume, "0") && strcmp(should_resume, "1")) {
        syslog(LOG_EMERG, "unrestrict: should_resume not 0 or 1");
        return 1;
    }

    /* double fork to avoid zombies */
    int ret = fork();
    if (ret == -1) {
        syslog(LOG_EMERG, "unrestrict: fork: %s", strerror(errno));
        return 1;
    } else if (ret) {
        return 0;
    }

    char *err = NULL;
    int sret = substitute_ios_unrestrict((pid_t) pid, should_resume[0] == '1', &err);
    if (sret) {
        syslog(LOG_EMERG, "unrestrict: substitute_ios_unrestrict => %d (%s)",
               sret, err);
        return 1;
    }

    return 0;
}
