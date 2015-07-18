#define IB_LOG_NAME "bundle-loader"
#define IB_LOG_TO_SYSLOG
#include "ib-log.h"
#include "darwin/mach-decls.h"
#include "darwin/xxpc.h"
#include "substitute-internal.h"
#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/mig.h>
#include <mach-o/loader.h>
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

static xxpc_connection_t substituted_conn;

static pthread_mutex_t hello_reply_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t hello_reply_cond = PTHREAD_COND_INITIALIZER;
static xxpc_object_t hello_reply;
static bool hello_reply_ready;

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
    if (!values)
        return BUNDLE_TEST_RESULT_EMPTY;
    if (xxpc_get_type(values) != XXPC_TYPE_ARRAY)
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
static bool check_bundle_with_info(xxpc_object_t info) {
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

static void notify_added_removed(const struct mach_header *mh32, bool is_add) {
    char id_dylib_buf[32];
    const char *id_dylib;

    const mach_header_x *mh = (void *) mh32;
    uint32_t ncmds = mh->ncmds;
    const struct load_command *lc = (void *) (mh + 1);
    for (uint32_t i = 0; i < ncmds; i++) {
        if (lc->cmd == LC_ID_DYLIB) {
            const struct dylib_command *dc = (void *) lc;
            id_dylib = (const char *) dc + dc->dylib.name.offset;
            goto ok;
        }
        lc = (void *) lc + lc->cmdsize;
    }
    /* no name? */
    sprintf(id_dylib_buf, "unknown.%p", mh32);
    id_dylib = id_dylib_buf;

ok:;
    xxpc_object_t message = xxpc_dictionary_create(NULL, NULL, 0);
    xxpc_dictionary_set_string(message, "type", "add-remove");
    xxpc_dictionary_set_bool(message, "is-add", is_add);
    xxpc_dictionary_set_string(message, "id-dylib", id_dylib);
    xxpc_connection_send_message(substituted_conn, message);
    xxpc_release(message);
}

static void add_image_cb(const struct mach_header *mh, intptr_t vmaddr_slide) {
    notify_added_removed(mh, true);

}

static void remove_image_cb(const struct mach_header *mh, intptr_t vmaddr_slide) {
    notify_added_removed(mh, false);
}

static bool handle_hello_reply(xxpc_object_t dict) {
    if (IB_VERBOSE) {
        char *desc = xxpc_copy_description(dict);
        ib_log("hello_reply: %s", desc);
        free(desc);
    }
    bool notify = xxpc_dictionary_get_bool(dict, "notify-me-of-add-remove");
    if (notify) {
        _dyld_register_func_for_add_image(add_image_cb);
        _dyld_register_func_for_remove_image(remove_image_cb);

        for (uint32_t i = 0, count = _dyld_image_count(); i < count; i++) {
            const struct mach_header *mh32 = _dyld_get_image_header(i);
            if (mh32)
                notify_added_removed(mh32, true);
        }
    }
    xxpc_object_t bundles = xxpc_dictionary_get_value(dict, "bundles");
    if (!bundles || xxpc_get_type(bundles) != XXPC_TYPE_ARRAY)
        return false;
    for (size_t i = 0, count = xxpc_array_get_count(bundles);
         i < count; i++) {
        if (!check_bundle_with_info(xxpc_array_get_value(bundles, i)))
            return false;
    }
    return true;
}

static void signal_hello_reply(xxpc_object_t object) {
    if (hello_reply_ready)
        return;
    pthread_mutex_lock(&hello_reply_mtx);
    hello_reply = object;
    hello_reply_ready = true;
    pthread_cond_signal(&hello_reply_cond);
    pthread_mutex_unlock(&hello_reply_mtx);
}

static void handle_xxpc_object(xxpc_object_t object, bool is_reply) {
    const char *msg;
    xxpc_type_t ty = xxpc_get_type(object);
    if (ty == XXPC_TYPE_DICTIONARY) {
        if (is_reply) {
            signal_hello_reply(xxpc_retain(object));
            return;
        }
        msg = "received extraneous message from substituted";
    } else if (ty == XXPC_TYPE_ERROR) {
        signal_hello_reply(NULL);
        msg = "XPC error communicating with substituted";
    } else {
        msg = "unknown object received from XPC";
    }
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
        goto bad;
    }

    substituted_conn = conn;

    xxpc_connection_set_event_handler(conn, ^(xxpc_object_t object) {
        handle_xxpc_object(object, false);
    });
    xxpc_connection_resume(conn);

    const char *argv0 = (*_NSGetArgv())[0];
    if (!argv0)
        argv0 = "???";

    xxpc_object_t message = xxpc_dictionary_create(NULL, NULL, 0);
    xxpc_dictionary_set_string(message, "type", "hello");
    xxpc_dictionary_set_int64(message, "proto-version", 1);
    xxpc_dictionary_set_string(message, "argv0", argv0);
    xxpc_connection_send_message_with_reply(conn, message, NULL,
                                           ^(xxpc_object_t reply) {
        handle_xxpc_object(reply, true);
    });
    xxpc_release(message);

    /* Timing out *always* means a bug (or the user manually unloaded
     * substituted).  Therefore, a high timeout is actually a good thing,
     * because it makes it clear (especially in testing) that something's wrong
     * rather than more subtly slowing things down.
     * We use mach_absolute_time + pthread_cond_timedwait_relative_np instead
     * of regular pthread_cond_timedwait in order to avoid issues if the system
     * time changes. */
    uint64_t then = mach_absolute_time() + 10 * NSEC_PER_SEC;
    pthread_mutex_lock(&hello_reply_mtx);
    while (!hello_reply_ready) {
        uint64_t now = mach_absolute_time();
        uint64_t remaining = now >= then ? 0 : then - now;
        struct timespec remaining_ts = {.tv_sec = remaining / NSEC_PER_SEC,
                                        .tv_nsec = remaining % NSEC_PER_SEC};
        int err = pthread_cond_timedwait_relative_np(&hello_reply_cond,
                                                     &hello_reply_mtx,
                                                     &remaining_ts);
        if (err != 0) {
            if (err == ETIMEDOUT)
                ib_log("ACK - didn't receive a reply from substituted in time!");
            else
                ib_log("pthread_cond_timedwait failed: %s", strerror(err));
            pthread_mutex_unlock(&hello_reply_mtx);
            goto bad;
        }
    }
    pthread_mutex_unlock(&hello_reply_mtx);

    if (hello_reply == NULL) {
        /* thread notified us of XPC error */
        goto bad;
    }

    if (!handle_hello_reply(hello_reply)) {
        char *desc = xxpc_copy_description(hello_reply);
        ib_log("received invalid message from substituted: %s", desc);
        free(desc);
        xxpc_release(hello_reply);
        goto bad;
    }
    xxpc_release(hello_reply);

    return;
bad:
    ib_log("giving up on loading bundles for this process...");
}
