#define IB_LOG_NAME "bundle-loader"
#define IB_LOG_TO_SYSLOG
#include "ib-log.h"
#include "darwin/mach-decls.h"
#include "xxpc.h"
#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/mig.h>
#include <objc/runtime.h>
#include <CoreFoundation/CoreFoundation.h>
#include <syslog.h>
#include <pthread.h>
#include <sys/time.h>
extern char ***_NSGetArgv(void);

static struct {
    bool initialized;
    typeof(CFBundleGetBundleWithIdentifier) *CFBundleGetBundleWithIdentifier;
    typeof(CFStringCreateWithCStringNoCopy) *CFStringCreateWithCStringNoCopy;
    typeof(CFRelease) *CFRelease;
    typeof(kCFAllocatorNull) *kCFAllocatorNull;
} cf_funcs;

static struct {
    bool initialized;
    typeof(objc_getClass) *objc_getClass;
} objc_funcs;

static pthread_mutex_t hello_reply_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t hello_reply_cond = PTHREAD_COND_INITIALIZER;
static xxpc_object_t hello_reply;

#define GET(funcs, handle, name) (funcs)->name = dlsym(handle, #name)

static void *dlopen_noload(const char *path) {
    void *h = dlopen(path, RTLD_LAZY | RTLD_NOLOAD);
    if (!h)
        return NULL;
    dlclose(h);
    return dlopen(path, RTLD_LAZY);
}

static bool cf_has_bundle(const char *name) {
    if (!cf_funcs.initialized) {
        const char *cf =
            "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation";
        void *handle = dlopen_noload(cf);
        if (IB_VERBOSE)
            ib_log("CF handle: %p", handle);
        if (handle) {
            GET(&cf_funcs, handle, CFBundleGetBundleWithIdentifier);
            GET(&cf_funcs, handle, CFStringCreateWithCStringNoCopy);
            GET(&cf_funcs, handle, CFRelease);
            GET(&cf_funcs, handle, kCFAllocatorNull);
        }
        cf_funcs.initialized = true;
    }
    if (!cf_funcs.CFBundleGetBundleWithIdentifier)
        return false;
    CFStringRef str = cf_funcs.CFStringCreateWithCStringNoCopy(
        NULL, name, kCFStringEncodingUTF8, *cf_funcs.kCFAllocatorNull);
    if (!str)
        return false;
    bool ret = !!cf_funcs.CFBundleGetBundleWithIdentifier(str);
    cf_funcs.CFRelease(str);
    return ret;
}

static bool objc_has_class(const char *name) {
    if (!objc_funcs.initialized) {
        void *handle = dlopen_noload("/usr/lib/libobjc.A.dylib");
        if (IB_VERBOSE)
            ib_log("objc handle: %p", handle);
        if (handle)
            GET(&objc_funcs, handle, objc_getClass);
        objc_funcs.initialized = true;
    }
    if (!objc_funcs.objc_getClass)
        return false;
    return !!objc_funcs.objc_getClass(name);
}

static void use_dylib(const char *name) {
    if (IB_VERBOSE)
        ib_log("loading dylib %s", name);
    dlopen(name, RTLD_LAZY);
}

enum bundle_test_result {
    BUNDLE_TEST_RESULT_INVALID,
    BUNDLE_TEST_RESULT_FAIL,
    BUNDLE_TEST_RESULT_PASS,
    BUNDLE_TEST_RESULT_EMPTY,
};

static enum bundle_test_result do_bundle_test_type(
    xxpc_object_t info, const char *key, bool (*test)(const char *)) {
    xxpc_object_t values = xxpc_dictionary_get_value(info, key);
    if (!values || xxpc_get_type(values) != XXPC_TYPE_ARRAY)
        return BUNDLE_TEST_RESULT_INVALID;
    size_t count = xxpc_array_get_count(values);
    if (count == 0)
        return BUNDLE_TEST_RESULT_EMPTY;
    for (size_t i = 0; i < count; i++) {
        const char *value = xxpc_array_get_string(values, i);
        if (!value)
            return BUNDLE_TEST_RESULT_INVALID;
        if (test(value))
            return BUNDLE_TEST_RESULT_PASS;
    }
    return BUNDLE_TEST_RESULT_FAIL;
}

/* return false if the info was invalid */
static bool load_bundle_with_info(xxpc_object_t info) {
    if (IB_VERBOSE) {
        char *desc = xxpc_copy_description(info);
        ib_log("load_bundle_with_info: %s", desc);
        free(desc);
    }
    bool any = xxpc_dictionary_get_bool(info, "any");
    enum bundle_test_result btr =
        do_bundle_test_type(info, "bundles", cf_has_bundle);
    if (btr == BUNDLE_TEST_RESULT_INVALID)
        return false;
    if (!any && btr == BUNDLE_TEST_RESULT_FAIL)
        goto no_load;
    if (any && btr == BUNDLE_TEST_RESULT_PASS)
        goto do_load;
    btr = do_bundle_test_type(info, "classes", objc_has_class);
    if (btr == BUNDLE_TEST_RESULT_INVALID)
        return false;
    if (btr == BUNDLE_TEST_RESULT_FAIL)
        goto no_load;
    else
        goto do_load;

no_load:
    return true;
do_load:;
    const char *name = xxpc_dictionary_get_string(info, "dylib");
    if (!name)
        return false;
    use_dylib(name);
    return true;
}

static bool handle_hello_reply(xxpc_object_t dict) {
    xxpc_object_t bundles = xxpc_dictionary_get_value(dict, "bundles");
    if (!bundles || xxpc_get_type(bundles) != XXPC_TYPE_ARRAY)
        return false;
    for (size_t i = 0, count = xxpc_array_get_count(bundles);
         i < count; i++) {
        if (!load_bundle_with_info(xxpc_array_get_value(bundles, i)))
            return false;
    }
    return true;
}

static void handle_xxpc_object(xxpc_object_t object, bool is_reply) {
    const char *msg;
    xxpc_type_t ty = xxpc_get_type(object);
    if (ty == XXPC_TYPE_DICTIONARY) {
        if (is_reply) {
            pthread_mutex_lock(&hello_reply_mtx);
            hello_reply = object;
            pthread_cond_signal(&hello_reply_cond);
            pthread_mutex_unlock(&hello_reply_mtx);
            return;
        }
        msg = "received extraneous message from substituted";
        goto complain;
    } else if (ty == XXPC_TYPE_ERROR) {
        msg = "XPC error communicating with substituted";
        goto complain;
    } else {
        msg = "unknown object received from XPC";
        goto complain;
    }
complain:;
    char *desc = xxpc_copy_description(object);
    ib_log("%s: %s", msg, desc);
    free(desc);
}

/* this is DYLD_INSERT_LIBRARIES'd, not injected. */
__attribute__((constructor))
static void init() {
    xxpc_connection_t conn = xxpc_connection_create_mach_service(
        "com.ex.substituted", NULL, 0);
    /* it's not supposed to return null, but just in case */
    if (!conn) {
        ib_log("xxpc_connection_create_mach_service returned null");
        return;
    }

    __block xxpc_object_t received_dict = NULL;
    __block bool did_receive_dict = false;

    xxpc_connection_set_event_handler(conn, ^(xxpc_object_t object) {
        handle_xxpc_object(object, false);
    });
    xxpc_connection_resume(conn);

    const char *argv0 = (*_NSGetArgv())[0];
    if (!argv0)
        argv0 = "???";

    xxpc_object_t message = xxpc_dictionary_create(NULL, NULL, 0);
    xxpc_dictionary_set_string(message, "type", "hello");
    xxpc_dictionary_set_string(message, "argv0", argv0);
    xxpc_connection_send_message_with_reply(conn, message, NULL,
                                           ^(xxpc_object_t reply) {
        handle_xxpc_object(reply, true);
    });

    /* Timing out *always* means a bug (or the user manually unloaded
     * substituted).  Therefore, a high timeout is actually a good thing,
     * because it makes it clear (especially in testing) that something's wrong
     * rather than more subtly slowing things down.
     * We use mach_absolute_time + pthread_cond_timedwait_relative_np instead
     * of regular pthread_cond_timedwait in order to avoid issues if the system
     * time changes. */
    uint64_t then = mach_absolute_time() + 10 * NSEC_PER_SEC;
    pthread_mutex_lock(&hello_reply_mtx);
    while (!hello_reply) {
        uint64_t now = mach_absolute_time();
        uint64_t remaining = now >= then ? 0 : then - now;
        struct timespec remaining_ts = {.tv_sec = remaining / NSEC_PER_SEC,
                                        .tv_nsec = remaining % NSEC_PER_SEC};
        if (pthread_cond_timedwait_relative_np(&hello_reply_cond, &hello_reply_mtx,
                                               &remaining_ts)) {
            if (errno == ETIMEDOUT) {
                ib_log("ACK - didn't receive a reply from substituted in time!");
                goto bad;
            } else {
                ib_log("pthread_cond_timedwait failed");
                goto bad;
            }
        }
    }
    pthread_mutex_unlock(&hello_reply_mtx);

    if (!handle_hello_reply(hello_reply)) {
        char *desc = xxpc_copy_description(hello_reply);
        ib_log("received invalid message from substituted: %s", desc);
        free(desc);
        goto bad;
    }

    return;
bad:
    ib_log("giving up...");
}
