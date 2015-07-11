#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#include "darwin/xxpc.h"
#include "substitute.h"

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
    bool _is_springboard, _got_bye;
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

- (bool)handleMessageHello:(xxpc_object_t)request {
    if (_argv0 != NULL)
        return false;

    const char *argv0 = xxpc_dictionary_get_string(request, "argv0");
    if (!argv0)
        return false;
    _argv0 = [NSString stringWithCString:argv0
                       encoding:NSUTF8StringEncoding];

    _is_springboard = [_argv0 isEqualToString:@"SpringBoard"];

    xxpc_object_t bundles = xxpc_array_create(NULL, 0);

    NSError *err;
    NSString *base = @"/Library/Substitute/DynamicLibraries";
    NSArray *list = [[NSFileManager defaultManager]
                     contentsOfDirectoryAtPath:base
                     error:&err];
    if (!list)
        return bundles;

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
    xxpc_connection_send_message(_connection, reply);
    return true;
}

- (bool)handleMessageBye:(xxpc_object_t)request {
    _got_bye = true;
    return true;
}

- (void)handleHangup {
    /* this could be false because hello hasn't been sent, but in that case it
     * hasn't loaded any substitute dylibs, so not our problem *whistle* */
    if (_is_springboard) {
        bool needs_safe = !_got_bye;
        if (needs_safe) {
            if (g_springboard_needs_safe) {
                NSLog(@"SpringBoard hung up more than once without without saying bye; using Really Safe Mode (no UI) next time :(");
                g_springboard_needs_safe = REALLY_SAFE;
            } else {
                NSLog(@"SpringBoard hung up without saying bye; using safe mode next time.");
                g_springboard_needs_safe = NEEDS_SAFE;
            }
        } else {
            g_springboard_needs_safe = NO_SAFE;
        }
    }
}


- (bool)handleMessage:(xxpc_object_t)request {
    const char *type = xxpc_dictionary_get_string(request, "type");
    if (!type)
        return false;
    if (!strcmp(type, "hello"))
        return [self handleMessageHello:request];
    else if (!strcmp(type, "bye"))
        return [self handleMessageBye:request];
    else
        return false;
}

- (instancetype)initWithConnection:(xxpc_object_t)connection {
    _connection = connection;
    xxpc_connection_set_event_handler(connection, ^(xxpc_object_t event) {
        xxpc_type_t ty = xxpc_get_type(event);
        if (ty == XXPC_TYPE_DICTIONARY) {
            if (![self handleMessage:event])
                xxpc_connection_cancel(connection);
        } else if (ty == XXPC_TYPE_ERROR) {
            if (event == XXPC_ERROR_CONNECTION_INVALID) {
                [self handleHangup];
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
    xxpc_connection_t listener = xxpc_connection_create_mach_service(
        "com.ex.substituted", NULL, XXPC_CONNECTION_MACH_SERVICE_LISTENER);
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
