#define IB_LOG_NAME "bundle-loader"
#define IB_LOG_TO_SYSLOG
#include "ib-log.h"
#include "darwin/mach-decls.h"
#include "substituted-messages.h"
#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/mig.h>
#include <objc/runtime.h>
#include <CoreFoundation/CoreFoundation.h>
#include <syslog.h>
extern char ***_NSGetArgv(void);

static int peek(const void *buf, const void *end) {
    if ((size_t) (end - buf) < sizeof(struct substituted_bundle_list_op))
        return -1;
    return ((struct substituted_bundle_list_op *) buf)->opc;
}

static bool pop(const void **buf, const void *end,
                int *opc,
                const char **name) {
    const struct substituted_bundle_list_op *op;
    if ((size_t) (end - *buf) < sizeof(*op))
        return false;
    op = *buf;
    *buf += sizeof(*op);
    *opc = op->opc;
    if ((size_t) (end - *buf) <= op->namelen ||
        ((const char *) buf)[op->namelen] != '\0')
        return false;
    *name = *buf;
    *buf += op->namelen + 1;
    return true;
}

static struct {
    bool initialized;
    typeof(CFBundleGetBundleWithIdentifier) *CFBundleGetBundleWithIdentifier;
    typeof(CFStringCreateWithCStringNoCopy) *CFStringCreateWithCStringNoCopy;
    typeof(CFRelease) *CFRelease;
    typeof(kCFAllocatorNull) *kCFAllocatorNull;
    typeof(kCFStringEncodingUTF8) *kCFStringEncodingUTF8;
} cf_funcs;

static struct {
    bool initialized;
    typeof(objc_getClass) *objc_getClass;
} objc_funcs;

#define GET(funcs, handle, name) (funcs)->name = dlsym(handle, "_" #name)

static bool cf_has_bundle(const char *name) {
    if (!cf_funcs.initialized) {
        void *handle = dlopen("/System/Library/Frameworks/CoreFoundation.framework",
                              RTLD_LAZY | RTLD_NOLOAD);
        if (handle) {
            GET(&cf_funcs, handle, CFBundleGetBundleWithIdentifier);
            GET(&cf_funcs, handle, CFStringCreateWithCStringNoCopy);
            GET(&cf_funcs, handle, CFRelease);
            GET(&cf_funcs, handle, kCFAllocatorNull);
            GET(&cf_funcs, handle, kCFStringEncodingUTF8);
        }
    }
    if (!cf_funcs.CFBundleGetBundleWithIdentifier)
        return false;
    CFStringRef str = cf_funcs.CFStringCreateWithCStringNoCopy(
        NULL, name, *cf_funcs.kCFStringEncodingUTF8, *cf_funcs.kCFAllocatorNull);
    if (!str)
        return false;
    bool ret = !!cf_funcs.CFBundleGetBundleWithIdentifier(str);
    cf_funcs.CFRelease(str);
    return ret;
}

static bool objc_has_class(const char *name) {
    if (!objc_funcs.initialized) {
        void *handle = dlopen("/usr/lib/libobjc.A.dylib",
                              RTLD_LAZY | RTLD_NOLOAD);
        if (handle)
            GET(&objc_funcs, handle, objc_getClass);
    }
    if (!objc_funcs.objc_getClass)
        return false;
    return !!objc_funcs.objc_getClass(name);
}

static void use_dylib(const char *name) {
    ib_log("loading dylib %s", name);
    // ..
}

static void load_bundle_list(const void *buf, size_t size) {
    if (IB_VERBOSE) {
        ib_log("load_bundle_list: %p,%zu", buf, size);
        ib_log_hex(buf, size);
    }
    int opc;
    const char *name;
    const void *end = buf + size;
    while (buf != end) {
        if (!pop(&buf, end, &opc, &name))
            goto invalid;
        bool pass;
        switch (opc) {
        case SUBSTITUTED_TEST_BUNDLE:
            pass = cf_has_bundle(name);
            if (IB_VERBOSE)
                ib_log("cf_has_bundle('%s'): %d", name, pass);
            if (pass)
                goto pass_type;
            /* fail, so... */
            if (peek(buf, end) != SUBSTITUTED_TEST_BUNDLE)
                goto fail;
            break;
        case SUBSTITUTED_TEST_CLASS:
            pass = objc_has_class(name);
            if (IB_VERBOSE)
                ib_log("objc_has_class('%s'): %d", name, pass);
            if (pass)
                goto pass_type;
            if (peek(buf, end) != SUBSTITUTED_TEST_CLASS)
                goto fail;
            break;
        case SUBSTITUTED_USE_DYLIB:
            use_dylib(name);
            break;
        pass_type:
            while (peek(buf, end) == opc) {
                if (!pop(&buf, end, &opc, &name))
                    goto invalid;
            }
        fail:
            do {
                if (!pop(&buf, end, &opc, &name))
                    goto invalid;
            } while (opc != SUBSTITUTED_USE_DYLIB);
            break;
        }
    }
    return;
invalid:
    ib_log("invalid bundle list data");
}

static kern_return_t substituted_hello(mach_port_t service, int proto_version,
                                       const char *argv0, void **bundle_list_p,
                                       size_t *bundle_list_size_p) {
    struct {
        mach_msg_header_t hdr;
        union {
            struct substituted_msg_body_hello req;
            struct substituted_msg_body_hello_resp resp;
        } u;
        mach_msg_trailer_t trailer_space;
    } buf;
    mach_port_t reply_port = mig_get_reply_port();
    buf.hdr.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,
                                       MACH_MSG_TYPE_MAKE_SEND_ONCE);
    buf.hdr.msgh_remote_port = service;
    buf.hdr.msgh_local_port = reply_port;
    buf.hdr.msgh_reserved = 0;
    buf.hdr.msgh_id = SUBSTITUTED_MSG_HELLO;
    buf.u.req.proto_version = proto_version;
    strlcpy(buf.u.req.argv0, argv0, sizeof(buf.u.req.argv0));
    size_t size = sizeof(buf.hdr) +
                  offsetof(struct substituted_msg_body_hello, argv0) +
                  strlen(buf.u.req.argv0);
    size_t round = round_msg(size);
    memset((char *) &buf + size, 0, round - size);
    buf.hdr.msgh_size = round;
    kern_return_t kr = mach_msg(&buf.hdr, MACH_RCV_MSG | MACH_SEND_MSG,
                                buf.hdr.msgh_size, sizeof(buf), reply_port,
                                MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if (kr) {
        if (kr == MACH_RCV_BODY_ERROR)
            mach_msg_destroy(&buf.hdr);
        return kr;
    }
    mach_msg_ool_descriptor_t *ool = &buf.u.resp.bundle_list_ool;

    if (buf.hdr.msgh_size != sizeof(buf.hdr) + sizeof(buf.u.resp) ||
        !(buf.hdr.msgh_bits & MACH_MSGH_BITS_COMPLEX) ||
        buf.u.resp.body.msgh_descriptor_count != 1 ||
        ool->type != MACH_MSG_OOL_DESCRIPTOR) {
        kr = KERN_INVALID_ARGUMENT;
        goto out;
    }

    if (buf.u.resp.error) {
        ib_log("substituted_hello returned error %x", buf.u.resp.error);
        kr = KERN_FAILURE;
        goto out;
    }

    *bundle_list_p = ool->address;
    *bundle_list_size_p = ool->size;
    return KERN_SUCCESS;

out:
    mach_msg_destroy(&buf.hdr);
    return kr;
}

/* this is DYLD_INSERT_LIBRARIES'd, not injected. */
__attribute__((constructor))
static void init() {
    mach_port_t service;
    kern_return_t kr = bootstrap_look_up(bootstrap_port, "com.ex.substituted",
                                         &service);
    if (kr) {
        ib_log("bootstrap_look_up com.ex.substituted: %x", kr);
        return;
    }
    const char *argv0 = (*_NSGetArgv())[0];
    void *bundle_list;
    size_t bundle_list_size;
    kr = substituted_hello(service, SUBSTITUTED_PROTO_VERSION, argv0,
                           &bundle_list, &bundle_list_size);
    if (kr) {
        ib_log("substituted_hello: %x", kr);
        return;
    }
    load_bundle_list(bundle_list, bundle_list_size);
    vm_deallocate(mach_task_self(), (vm_address_t) bundle_list, bundle_list_size);

    /* hang onto the port */
}
