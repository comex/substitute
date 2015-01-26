#include "substitute.h"
#include "substitute-internal.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(int argc, char **argv) {
    if (argc <= 2) {
        printf("usage: test-inject <pid> <dylib>\n");
        return 1;
    }
    int pid = atoi(argv[1]);
    char *error;
    mach_port_t port = 0;
    assert(!mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port));
    struct shuttle shuttles[] = {
        {.type = SUBSTITUTE_SHUTTLE_MACH_PORT,
         .u.mach.port = port,
         .u.mach.right_type = MACH_MSG_TYPE_MAKE_SEND}
    };
    int ret = substitute_dlopen_in_pid(pid, argv[2], 0, shuttles, 1, &error);
    printf("ret=%d err=%s\n", ret, error);
    assert(!ret);
    free(error);
    static struct {
        mach_msg_header_t hdr;
        char body[5];
    } msg;
    assert(!mach_msg_overwrite(NULL, MACH_RCV_MSG, 0, sizeof(msg), port,
                               MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL, &msg.hdr, 0));
    printf("received '%.5s'\n", msg.body);

}
