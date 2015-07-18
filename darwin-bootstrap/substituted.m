#include "darwin/xxpc.h"
#include "substitute.h"
#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <mach/vm_param.h>

void *vproc_swap_complex(void *vp, int key, xxpc_object_t inval,
                         __strong xxpc_object_t *outval);

/* This is a daemon contacted by all processes which can load extensions.  It
 * currently does the work of reading the plists in
 * /Library/Substitute/DynamicLibraries in order to avoid loading objc/CF
 * libraries into the target binary (unless actually required by loaded
 * libraries).  In the future it will help with hot loading. */

static enum {
    NO_SAFE,
    NEEDS_SAFE,
    REALLY_SAFE,
} g_springboard_needs_safe;

static NSSet *g_springboard_loaded_dylibs;
static NSSet *g_springboard_last_loaded_dylibs;

static bool load_state() {
    int fd = shm_open("com.ex.substituted.state", O_RDONLY);
    if (fd == -1)
        return false;
    struct stat st;
    if (fstat(fd, &st)) {
        NSLog(@"fstat error");
        return false;
    }
    size_t size = (size_t) st.st_size;
    /* note: can't have concurrent modification */
    void *buf = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (buf == MAP_FAILED) {
        NSLog(@"mmap error");
        return false;
    }
    NSData *nsd = [NSData dataWithBytesNoCopy:buf length:size];
    NSError *err = nil;
    id result = [NSPropertyListSerialization propertyListWithData:nsd
                                             options:NSPropertyListImmutable
                                             format:NULL error:&err];
    if (err)
        NSLog(@"propertyListWithData: %@", err);
    if (!result)
        return false;
    if (![result isKindOfClass:[NSSet class]]) {
        NSLog(@"loaded data is not NSSet - newer version?");
        return false;
    }
    for (id str in result) {
        if (![str isKindOfClass:[NSString class]]) {
            NSLog(@"loaded data member is not NSString");
            return false;
        }
    }
    g_springboard_last_loaded_dylibs = result;
    munmap(buf, size);
    close(fd);
    return true;
}

static bool save_state() {
    NSError *err = nil;
    NSData *data = [NSPropertyListSerialization
                    dataWithPropertyList:g_springboard_last_loaded_dylibs
                    format:NSPropertyListBinaryFormat_v1_0 options:0
                    error:&err];
    if (err)
        NSLog(@"dataWithPropertyList: %@", err);
    if (!data)
        return false;
    int fd = shm_open("com.ex.substituted.state", O_RDWR | O_CREAT | O_TRUNC,
                      0644);
    if (fd == -1) {
        NSLog(@"shm_open error (write)");
        return false;
    }
    size_t size = ([data length] + PAGE_MASK) & ~PAGE_MASK;
    if (ftruncate(fd, size)) {
        NSLog(@"ftruncate error (%zu)", size);
        return false;
    }

    void *buf = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (buf == MAP_FAILED) {
        NSLog(@"mmap error (write)");
        return false;
    }

    memcpy(buf, [data bytes], [data length]);
    munmap(buf, size);
    close(fd);
    return true;
}

extern kern_return_t bootstrap_look_up3(mach_port_t bp,
    const char *service_name, mach_port_t *sp, pid_t target_pid,
    const uuid_t instance_id, uint64_t flags);

static kern_return_t my_bootstrap_look_up3(mach_port_t bp,
    const char *service_name, mach_port_t *sp, pid_t target_pid,
    const uuid_t instance_id, uint64_t flags) {
    NSLog(@"Something in substituted tried to look up '%s', which could cause a deadlock.  This is a bug.",
          service_name);
    return KERN_FAILURE;
}
static const struct substitute_function_hook deadlock_warning_hook = {
    bootstrap_look_up3, my_bootstrap_look_up3, NULL
};

static void install_deadlock_warning() {
    int ret = substitute_hook_functions(&deadlock_warning_hook, 1, NULL, 0);
    if (ret) {
        NSLog(@"substitute_hook_functions(&deadlock_warning_hook, ..) failed: %d",
              ret);
    }
}

static double id_to_double(id o) {
    if ([o isKindOfClass:[NSString class]]) {
        NSScanner *scan = [NSScanner scannerWithString:o];
        double d;
        if (![scan scanDouble:&d] || !scan.atEnd)
            return NAN;
        return d;
    } else if ([o isKindOfClass:[NSNumber class]]) {
        return [o doubleValue];
    } else {
        return NAN;
    }
}

static xxpc_object_t nsstring_to_xpc(NSString *in) {
    return xxpc_string_create([in cStringUsingEncoding:NSUTF8StringEncoding]);
}

@interface PeerHandler : NSObject {
    xxpc_object_t _connection;
    NSString *_argv0;
    bool _is_springboard;
    NSMutableSet *_loaded_dylibs;
}

@end

enum convert_filters_ret TEST;

@implementation PeerHandler

enum convert_filters_ret {
    PROVISIONAL_PASS,
    FAIL,
    INVALID
};

- (enum convert_filters_ret)
    convertFiltersForBundleInfo:(NSDictionary *)plist_dict
    toXPCReply:(xxpc_object_t)out_info {

    for (NSString *key in [plist_dict allKeys]) {
        if (!([key isEqualToString:@"Filter"]))
            return INVALID;
    }

    NSDictionary *filter = [plist_dict objectForKey:@"Filter"];
    if (!filter)
        return PROVISIONAL_PASS;

    if (![filter isKindOfClass:[NSDictionary class]])
        return INVALID;

    for (NSString *key in [filter allKeys]) {
        if (!([key isEqualToString:@"CoreFoundationVersion"] ||
              [key isEqualToString:@"Classes"] ||
              [key isEqualToString:@"Bundles"] ||
              [key isEqualToString:@"Executables"] ||
              [key isEqualToString:@"Mode"] ||
              [key isEqualToString:@"SafeMode"])) {
            return INVALID;
        }
    }

    bool for_safe_mode = false;
    NSNumber *safe_mode_num = [filter objectForKey:@"SafeMode"];
    if (safe_mode_num) {
         if ([safe_mode_num isEqual:[NSNumber numberWithBool:true]])
            for_safe_mode = true;
         else if (![safe_mode_num isEqual:[NSNumber numberWithBool:false]])
            return INVALID;
    }
    /* in REALLY_SAFE mode, nothing gets loaded */
    if (for_safe_mode) {
        if (!_is_springboard || g_springboard_needs_safe != NEEDS_SAFE)
            return FAIL;
    } else {
        if (_is_springboard && g_springboard_needs_safe != NO_SAFE)
            return FAIL;
    }

    bool any = false;
    NSString *mode_str = [filter objectForKey:@"Mode"];
    if (mode_str) {
        any = [mode_str isEqual:@"Any"];
        if (!any && ![mode_str isEqual:@"All"])
            return INVALID;
    }

    xxpc_dictionary_set_bool(out_info, "any", any);

    /* First do the two we can test here. */

    NSArray *cfv = [filter objectForKey:@"CoreFoundationVersion"];
    if (cfv) {
        if (![cfv isKindOfClass:[NSArray class]] ||
            [cfv count] == 0 ||
            [cfv count] > 2)
            return INVALID;
        double version = kCFCoreFoundationVersionNumber;
        double minimum = id_to_double([cfv objectAtIndex:0]);
        if (minimum != minimum)
            return INVALID;
        if (version < minimum)
            return FAIL;
        id supremum_o = [cfv objectAtIndex:1];
        if (supremum_o) {
            double supremum = id_to_double(supremum_o);
            if (supremum != supremum)
                return INVALID;
            if (version >= supremum)
                return FAIL;
        }
    }

    NSArray *executables = [filter objectForKey:@"Executables"];
    if (executables) {
        if (![executables isKindOfClass:[NSArray class]])
            return INVALID;
        for (NSString *name in executables) {
            if (![name isKindOfClass:[NSString class]])
                return INVALID;
            if ([name isEqualToString:_argv0])
                goto ok;
        }
        if (any)
            return PROVISIONAL_PASS; /* without adding other conditions */
        else
            return FAIL;
    ok:;
    }

    /* Convert the rest to tests for bundle-loader. */

    struct {
        __unsafe_unretained NSString *key;
        const char *okey;
    } types[2] = {
        {@"Classes", "classes"},
        {@"Bundles", "bundles"},
    };

    for (int i = 0; i < 2; i++) {
        NSArray *things = [filter objectForKey:types[i].key];
        if (things) {
            if (![things isKindOfClass:[NSArray class]])
                return INVALID;
            xxpc_object_t out_things = xxpc_array_create(NULL, 0);
            for (NSString *name in things) {
                if (![name isKindOfClass:[NSString class]])
                    return INVALID;
                xxpc_array_append_value(out_things, nsstring_to_xpc(name));
            }
            xxpc_dictionary_set_value(out_info, types[i].okey, out_things);
        }
    }

    return PROVISIONAL_PASS;
}

- (void)updateSpringBoardNeedsSafeThen:(void (^)())then {
    xxpc_object_t inn = xxpc_dictionary_create(NULL, NULL, 0);
    xxpc_dictionary_set_string(inn, "com.ex.substitute.hook-operation",
                               "bundleid-to-fate");
    xxpc_dictionary_set_string(inn, "bundleid", "com.apple.SpringBoard");

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                   ^{
        int to_set = REALLY_SAFE;
        xxpc_object_t out = NULL;
        vproc_swap_complex(NULL, 99999, inn, &out);
        if (!out) {
            NSLog(@"couldn't talk to launchd :( - assume worst case scenario");
            goto out;
        }
        __unsafe_unretained xxpc_type_t type = xxpc_get_type(out);
        if (xxpc_get_type(out) != XXPC_TYPE_DICTIONARY)
            goto bad_data;

        bool crashed;
        __unsafe_unretained xxpc_object_t fate =
            xxpc_dictionary_get_value(out, "fate");
        if (!fate) {
            /* no record yet */
            crashed = false;
        } else if (xxpc_get_type(fate) == XXPC_TYPE_INT64) {
            int stat = (int) xxpc_int64_get_value(fate);
            crashed = WIFSIGNALED(stat) && WTERMSIG(stat) != SIGTERM;
        } else {
            goto bad_data;
        }


        if (crashed) {
            if (g_springboard_needs_safe) {
                NSLog(@"SpringBoard crashed while in safe mode; using Really Safe Mode (no UI) next time :(");
                to_set = REALLY_SAFE;
            } else {
                NSLog(@"SpringBoard crashed; using safe mode next time.");
                to_set = NEEDS_SAFE;
            }
        } else {
            to_set = NO_SAFE;
        }
        goto out;

    bad_data:
        NSLog(@"bad data %@ from launchd!?", out);
        goto out;
    out:
        dispatch_async(dispatch_get_main_queue(), ^{
            g_springboard_needs_safe = to_set;
            then();
        });
    });
}

- (void)handleMessageHello:(NS_VALID_UNTIL_END_OF_SCOPE xxpc_object_t)request {
    NSString *sb_exe =
        @"/System/Library/CoreServices/SpringBoard.app/SpringBoard";

    if (_argv0 != NULL)
        goto bad;

    int64_t version = xxpc_dictionary_get_int64(request, "proto-version");
    if (version != 1) {
        /* in the future there will be a proper unloading mechanism, but here's
         * a bit of future proofing */
        NSLog(@"request received from wrong version of bundle-loader: %@", request);
        xxpc_connection_cancel(_connection);
    }

    const char *argv0 = xxpc_dictionary_get_string(request, "argv0");
    if (!argv0)
        goto bad;
    _argv0 = [NSString stringWithCString:argv0
                       encoding:NSUTF8StringEncoding];

    _is_springboard = [_argv0 isEqualToString:sb_exe];

    if (_is_springboard) {
        g_springboard_last_loaded_dylibs = g_springboard_loaded_dylibs;
        g_springboard_loaded_dylibs = _loaded_dylibs = [NSMutableSet set];
        save_state();

        [self updateSpringBoardNeedsSafeThen:^{
            [self handleMessageHelloRest:request];
        }];
    } else {
        [self handleMessageHelloRest:request];
    }
    return;

bad:
    [self handleBadMessage:request];
}

- (void)handleMessageHelloRest:(NS_VALID_UNTIL_END_OF_SCOPE
                                xxpc_object_t)request {

    xxpc_object_t bundles = xxpc_array_create(NULL, 0);

    NSError *err;
    NSString *base = @"/Library/Substitute/DynamicLibraries";
    NSArray *list = [[NSFileManager defaultManager]
                     contentsOfDirectoryAtPath:base
                     error:&err];

    for (NSString *dylib in list) {
        if (![[dylib pathExtension] isEqualToString:@"dylib"])
            continue;
        NSString *plist = [[dylib stringByDeletingPathExtension]
                           stringByAppendingPathExtension:@"plist"];
        NSString *full_plist = [base stringByAppendingPathComponent:plist];
        NSDictionary *plist_dict = [NSDictionary dictionaryWithContentsOfFile:
                                    full_plist];
        if (!plist_dict) {
            NSLog(@"missing, unreadable, or invalid plist '%@' for dylib '%@'; unlike Substrate, we require plists",
                  full_plist, dylib);
            continue;
        }

        xxpc_object_t info = xxpc_dictionary_create(NULL, NULL, 0);

        enum convert_filters_ret ret =
            [self convertFiltersForBundleInfo:plist_dict toXPCReply:info];
        if (ret == FAIL) {
            continue;
        } else if (ret == INVALID) {
            NSLog(@"bad data in plist '%@' for dylib '%@'", full_plist, dylib);
            continue;
        }

        NSString *dylib_path = [base stringByAppendingPathComponent:dylib];
        xxpc_dictionary_set_value(info, "dylib", nsstring_to_xpc(dylib_path));
        xxpc_array_append_value(bundles, info);
    }

    xxpc_object_t reply = xxpc_dictionary_create_reply(request);
    xxpc_dictionary_set_value(reply, "bundles", bundles);
    if (_is_springboard)
        xxpc_dictionary_set_bool(reply, "notify-me-of-add-remove", true);
    xxpc_connection_send_message(_connection, reply);
}

- (void)handleMessageAddRemove:(NS_VALID_UNTIL_END_OF_SCOPE
                                xxpc_object_t)request {
    bool is_add = xxpc_dictionary_get_bool(request, "is-add");
    const char *id_dylib = xxpc_dictionary_get_string(request, "id-dylib");
    if (!id_dylib || !_loaded_dylibs)
        return [self handleBadMessage:request];
    NSString *id_dylib_s = [NSString stringWithCString:id_dylib
                                     encoding:NSUTF8StringEncoding];
    if (is_add)
        [_loaded_dylibs addObject:id_dylib_s];
    else
        [_loaded_dylibs removeObject:id_dylib_s];
}

- (void)handleMessageSBFatalLoadedDylibs:(xxpc_object_t)request {
    /* This should probably be secured somehow... */
    NSLog(@"handleMessageSBFatalLoadedDylibs");
    NSSet *set = g_springboard_last_loaded_dylibs;
    xxpc_object_t reply = xxpc_dictionary_create_reply(request);
    if (set) {
        xxpc_object_t dylibs = xxpc_array_create(NULL, 0);
        xxpc_dictionary_set_value(reply, "dylibs", dylibs);
        for (NSString *dylib in set) {
            xxpc_array_set_string(dylibs, XXPC_ARRAY_APPEND,
                                  [dylib cStringUsingEncoding:
                                         NSUTF8StringEncoding]);

        }
    }
    xxpc_connection_send_message(_connection, reply);
}

- (void)handleBadMessage:(xxpc_object_t)request {
    NSLog(@"bad message received: %@", request);
    xxpc_connection_cancel(_connection);
}

- (void)handleMessage:(NS_VALID_UNTIL_END_OF_SCOPE xxpc_object_t)request {
    const char *type = xxpc_dictionary_get_string(request, "type");
    if (!type)
        goto bad;
    if (!strcmp(type, "hello"))
        return [self handleMessageHello:request];
    else if (!strcmp(type, "add-remove"))
        return [self handleMessageAddRemove:request];
    else if (!strcmp(type, "springboard-fatal-loaded-dylibs"))
        return [self handleMessageSBFatalLoadedDylibs:request];
    else
        goto bad;
bad:
    return [self handleBadMessage:request];
}

- (instancetype)initWithConnection:(xxpc_object_t)connection {
    _connection = connection;
    xxpc_connection_set_event_handler(connection, ^(xxpc_object_t event) {
        xxpc_type_t ty = xxpc_get_type(event);
        if (ty == XXPC_TYPE_DICTIONARY) {
            [self handleMessage:event];
        } else if (ty == XXPC_TYPE_ERROR) {
            if (event == XXPC_ERROR_CONNECTION_INVALID) {
                /* [self handleHangup]; */
            } else {
                NSLog(@"XPC error from connection: %@", event);
                xxpc_connection_cancel(connection);
            }
        } else {
            NSLog(@"unknown object received from XPC (peer): %@", event);
        }
    });
    xxpc_connection_resume(connection);
    return self;
}
@end

int main() {
    NSLog(@"hello from substituted");
    install_deadlock_warning();
    load_state();
    xxpc_connection_t listener = xxpc_connection_create_mach_service(
        "com.ex.substituted", dispatch_get_main_queue(),
        XXPC_CONNECTION_MACH_SERVICE_LISTENER);
    if (!listener) {
        NSLog(@"xxpc_connection_create_mach_service returned null");
        exit(1);
    }
    xxpc_connection_set_event_handler(listener, ^(xxpc_object_t object) {
        xxpc_type_t ty = xxpc_get_type(object);
        if (ty == XXPC_TYPE_CONNECTION) {
            (void) [[PeerHandler alloc] initWithConnection:object];
        } else if (ty == XXPC_TYPE_ERROR) {
            NSLog(@"XPC error in server: %@", object);
            exit(1);
        } else {
            NSLog(@"unknown object received from XPC: %@", object);
        }
    });
    xxpc_connection_resume(listener);
    [[NSRunLoop mainRunLoop] run];
}
