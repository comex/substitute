#include "substitute.h"
#include "substitute-internal.h"
#include <stdlib.h>
#include <syslog.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        syslog(LOG_EMERG, "unrestrict-me: wrong number of args");
        return 1;
    }
    const char *pids = argv[1];
    char *end;
    long pid = strtol(pids, &end, 10);
    if (!pids[0] || *end) {
        syslog(LOG_EMERG, "unrestrict-me: pid not an integer");
        return 1;
    }

    const char *should_resume = argv[2];
    if (strcmp(should_resume, "0") && strcmp(should_resume, "1")) {
        syslog(LOG_EMERG, "unrestrict-me: should_resume not 0 or 1");
        return 1;
    }

    int sret = substitute_ios_unrestrict((pid_t) pid, should_resume[0] == '1');
    if (sret) {
        syslog(LOG_EMERG, "unrestrict-me: substitute_ios_unrestrict => %d", sret);
        return 1;
    }

    return 0;
}
