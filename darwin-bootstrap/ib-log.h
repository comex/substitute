#pragma once
#include <dispatch/dispatch.h>
#include <stdio.h>
#include <unistd.h>

static FILE *logfp;
static void open_logfp_if_necessary() {
    /* syslog() doesn't seem to work from launchd... */
    static dispatch_once_t pred;
    dispatch_once(&pred, ^{
        char filename[128];
        sprintf(filename, "/tmp/substitute-" IB_LOG_NAME "-log.%ld",
                (long) getpid());
        logfp = fopen(filename, "w");
        if (!logfp) {
            /* Ack... */
            logfp = stderr;
        }
    });
}
#define ib_log(fmt, args...) do { \
    open_logfp_if_necessary(); \
    fprintf(logfp, fmt "\n", ##args); \
    fflush(logfp); \
} while(0)

#define IB_VERBOSE 0
