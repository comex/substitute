#include "substitute.h"
#include "substitute-internal.h"
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <stdbool.h>
#include <stdint.h>
#include <syslog.h>
#include <CoreFoundation/CoreFoundation.h>

void *IOHIDEventCreateKeyboardEvent(CFAllocatorRef, uint64_t, uint32_t, uint32_t, bool, uint32_t);
void *IOHIDEventSystemCreate(CFAllocatorRef);
void *IOHIDEventSystemCopyEvent(void *, uint32_t, void *, uint32_t);

CFIndex IOHIDEventGetIntegerValue(void *, uint32_t);
enum {
    kIOHIDEventTypeKeyboard = 3,
    kIOHIDEventFieldKeyboardDown = 3 << 16 | 2,
};

static bool button_pressed(uint32_t usage_page, uint32_t usage) {
    /* This magic comes straight from Substrate... I don't really understand
     * what it's doing.  In particular, where is the equivalent kernel
     * implementation on OS X?  Does it not exist?  But I guess Substrate is
     * emulating backboardd. */
    void *dummy = IOHIDEventCreateKeyboardEvent(NULL, mach_absolute_time(),
                                                usage_page, usage,
                                                0, 0);
    if (!dummy) {
        syslog(LOG_EMERG, "couldn't create dummy HID event");
        return false;
    }
    void *event_system = IOHIDEventSystemCreate(NULL);
    if (!event_system) {
        syslog(LOG_EMERG, "couldn't create HID event system");
        return false;
    }
    void *event = IOHIDEventSystemCopyEvent(event_system,
                                            kIOHIDEventTypeKeyboard,
                                            dummy, 0);
    if (!event) {
        syslog(LOG_EMERG, "couldn't copy HID event");
        return false;
    }
    CFIndex ival = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown);
    return ival;
}

int main(UNUSED int argc, char **argv) {
    pid_t pid = argv[1] ? atoi(argv[1]) : 1; /* for testing */

    if (button_pressed(0x0c, 0xe9) || /* consumer page -> Volume Increment */
        button_pressed(0x0b, 0x21)) { /* telephony page -> Flash */
        syslog(LOG_WARNING, "disabling due to button press");
        return 0;
    }
    mach_port_t port = 0;
    kern_return_t kr = mach_port_allocate(mach_task_self(),
                                          MACH_PORT_RIGHT_RECEIVE,
                                          &port);
    if (kr) {
        syslog(LOG_EMERG, "mach_port_allocate: %x", kr);
        return 0;
    }
    const char *lib = "/Library/Substitute/posixspawn-hook.dylib";
    struct shuttle shuttle = {
        .type = SUBSTITUTE_SHUTTLE_MACH_PORT,
        .u.mach.right_type = MACH_MSG_TYPE_MAKE_SEND,
        .u.mach.port = port
    };
    char *error;
    int ret = substitute_dlopen_in_pid(pid, lib, 0, &shuttle, 1, &error);
    if (ret) {
        syslog(LOG_EMERG, "substitute_dlopen_in_pid: %s/%s",
               substitute_strerror(ret), error);
        return 0;
    }
    /* wait for it to finish */
    static struct {
        mach_msg_header_t hdr;
        mach_msg_trailer_t huh;
    } msg;
    kr = mach_msg_overwrite(NULL, MACH_RCV_MSG, 0, sizeof(msg), port,
                            MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL,
                            &msg.hdr, 0);
    if (kr)
        syslog(LOG_EMERG, "mach_msg_overwrite: %x", kr);
}
