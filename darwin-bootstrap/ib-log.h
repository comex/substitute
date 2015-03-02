#pragma once
#include <dispatch/dispatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef IB_LOG_TO_SYSLOG
#include <syslog.h>
#define ib_log(fmt, args...) syslog(LOG_ERR, IB_LOG_NAME ": " fmt, ##args)
#else
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
#endif

static inline void ib_log_hex(const void *buf, size_t size) {
    const uint8_t *up = buf;
    char *hex = malloc(2 * size + 1), *p = hex;
    for (size_t i = 0; i < size; i++) {
        sprintf(p, "%02x", up[i]);
        p += 2;
    }
    ib_log("%s", hex);
    free(hex);
}

#define IB_VERBOSE 1
