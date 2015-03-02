#pragma once
#include <mach/mach.h>
#include <sys/param.h>

enum {
    SUBSTITUTED_PROTO_VERSION = 1
};

enum substituted_msg_id {
    SUBSTITUTED_MSG_HELLO = 10000,
    SUBSTITUTED_MSG_HELLO_RESP = 10001,
};

struct substituted_msg_body_hello {
    int proto_version;
    char argv0[/*0..*/MAXPATHLEN];
};

struct substituted_msg_body_hello_resp {
    mach_msg_body_t body;
    mach_msg_ool_descriptor_t bundle_list_ool;
    int error;
};

/* bundle_list: a bunch of substituted_bundle_list_ops
 * this is pretty silly because even low-level bootstrap_lookup uses xpc now -
 * so could have just used xpc structures - but this is more fun */

enum substituted_bundle_list_opc {
    SUBSTITUTED_TEST_BUNDLE,
    SUBSTITUTED_TEST_CLASS,
    SUBSTITUTED_USE_DYLIB,
};

struct substituted_bundle_list_op {
    uint16_t namelen;
    uint8_t opc;
    /* char name[namelen + 1]; */
} __attribute__((packed));
