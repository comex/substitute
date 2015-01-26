#include "substitute-internal.h"
#include <stdio.h>
#include <mach/mach.h>
#include <assert.h>
__attribute__((constructor))
static void hi() {
    printf("constructor\n");
}

void substitute_init(struct shuttle *shuttle, size_t nshuttle) {
    printf("substitute_init nshuttle=%zd\n", nshuttle);
    assert(nshuttle == 1);
    assert(shuttle[0].type == SUBSTITUTE_SHUTTLE_MACH_PORT);
    struct {
        mach_msg_header_t hdr;
        char body[5];
    } msg;
    msg.hdr.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    msg.hdr.msgh_size = sizeof(msg);
    msg.hdr.msgh_remote_port = shuttle[0].u.mach.port;
    msg.hdr.msgh_local_port = 0;
    msg.hdr.msgh_voucher_port = 0;
    msg.hdr.msgh_id = 42;
    strncpy(msg.body, "hello", 5);
    assert(!mach_msg_send(&msg.hdr));
}
