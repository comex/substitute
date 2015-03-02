/* This is a daemon contacted by all processes which can load extensions.  It
 * currently does the work of reading the plists in
 * /Library/Substitute/DynamicLibraries in order to avoid loading objc/CF
 * libraries into the target binary (unless actually required by loaded
 * libraries).  By itself this would not merit the overhead of a separate
 * daemon, but in the future this will also coordinate hot loading and
 * unloading, for which purpose I think a daemon would be a cleaner solution
 * than task_for_pid'ing everything in sight. */

#define IB_LOG_NAME "substituted"
#include "darwin/mach-decls.h"
#include "ib-log.h"
#include "substituted-messages.h"
#include "substituted-plist-loader.h"
#include <mach/mach.h>
#include <stddef.h>
//#include <sys/types.h>

/* libbsm.h */
struct au_tid;
void audit_token_to_au32(audit_token_t, uid_t *, uid_t *, gid_t *, uid_t *,
                         gid_t *, pid_t *, pid_t *, struct au_tid *);

struct msgbuf {
    mach_msg_header_t hdr;
    union {
        struct substituted_msg_body_hello hello;
    } u;
    mach_msg_audit_trailer_t trail_space;
};

static void handle_hello(struct msgbuf *buf, pid_t pid) {
    if (buf->hdr.msgh_size < offsetof(struct msgbuf, u.hello.argv0)) {
        ib_log("message too short");
        return;
    }
    ((char *) buf)[buf->hdr.msgh_size] = '\0'; /* overwrite trailer or whatever */
    const char *name = buf->u.hello.argv0;
    if (IB_VERBOSE)
        ib_log("got hello from pid %d [%s]", pid, name);
    int error = 0;
    const void *bundle_list = NULL;
    size_t bundle_list_size = 0;
    if (buf->u.hello.proto_version != SUBSTITUTED_PROTO_VERSION) {
        error = 1;
    } else {
        get_bundle_list(name, &bundle_list, &bundle_list_size);
    }
    struct {
        mach_msg_header_t hdr;
        struct substituted_msg_body_hello_resp b;
    } resp;
    resp.hdr.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0) |
                         MACH_MSGH_BITS_COMPLEX;
    resp.hdr.msgh_size = sizeof(resp);
    resp.hdr.msgh_remote_port = buf->hdr.msgh_remote_port;
    resp.hdr.msgh_local_port = MACH_PORT_NULL;
    resp.hdr.msgh_reserved = 0;
    resp.hdr.msgh_id = SUBSTITUTED_MSG_HELLO_RESP;
    resp.b.body.msgh_descriptor_count = 1;
    mach_msg_ool_descriptor_t *ool = &resp.b.bundle_list_ool;
    ool->pad1 = 0;
    ool->type = MACH_MSG_OOL_DESCRIPTOR;
    ool->deallocate = 0;
    ool->copy = MACH_MSG_PHYSICAL_COPY;
    ool->address = (void *) bundle_list;
    ool->size = (mach_msg_size_t) bundle_list_size;
    resp.b.error = error;

    kern_return_t kr = mach_msg(&resp.hdr, MACH_SEND_MSG, sizeof(resp), 0,
                                MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE,
                                MACH_PORT_NULL);
    if (kr)
        ib_log("mach_msg(hello resp) -> %x", kr);
    else /* don't re-destroy the moved right */
        buf->hdr.msgh_remote_port = MACH_PORT_NULL;
}

int main() {
    mach_port_t service;
    kern_return_t kr = bootstrap_check_in(bootstrap_port,
                                          "com.ex.substituted",
                                          &service);
    while (1) {
        struct msgbuf buf;
        mach_msg_option_t option =
            MACH_RCV_MSG |
            MACH_RCV_TRAILER_TYPE(MACH_RCV_TRAILER_AUDIT) |
            MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT);

        kr = mach_msg(&buf.hdr, option, 0, sizeof(buf), service,
                      MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
        switch (kr) {
        case MACH_MSG_SUCCESS:
            break;
        case MACH_RCV_BODY_ERROR:
            mach_msg_destroy(&buf.hdr);
            /* fallthrough */
        case MACH_RCV_TOO_LARGE:
        case MACH_RCV_HEADER_ERROR:
        case MACH_RCV_SCATTER_SMALL:
            ib_log("mach_msg(rcv) -> %x", kr);
            continue;
        default:
            ib_log("mach_msg(rcv) -> %x (fatal)", kr);
            return 1;
        }

        mach_msg_audit_trailer_t *real_trail = (void *) &buf.hdr +
                                               round_msg(buf.hdr.msgh_size);

        pid_t pid;
        audit_token_to_au32(real_trail->msgh_audit, NULL, NULL, NULL, NULL,
                            NULL, &pid, NULL, NULL);

        switch (buf.hdr.msgh_id) {
        case SUBSTITUTED_MSG_HELLO:
            handle_hello(&buf, pid);
            break;
        default:
            ib_log("unknown message %d", buf.hdr.msgh_id);
            break;
        }

        mach_msg_destroy(&buf.hdr);
    }
}
