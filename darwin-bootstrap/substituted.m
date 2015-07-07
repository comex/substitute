#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#include "xxpc.h"

enum convert_filters_ret {
    PROVISIONAL_PASS,
    FAIL,
    INVALID
};

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

static enum convert_filters_ret convert_filters(NSDictionary *plist_dict,
                                                const char *exec_name,
                                                xxpc_object_t out_info) {

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
              [key isEqualToString:@"Mode"])) {
            return INVALID;
        }
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
        NSString *exe = [NSString stringWithCString:exec_name
                                  encoding:NSUTF8StringEncoding];
        if (![executables isKindOfClass:[NSArray class]])
            return INVALID;
        for (NSString *name in executables) {
            if (![name isKindOfClass:[NSString class]])
                return INVALID;
            if ([name isEqualToString:exe])
                goto ok;
        }
        if (any)
            return PROVISIONAL_PASS; // without adding other conditions
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
            xxpc_object_t out_things = xxpc_array_create(NULL, 0);
            if (![things isKindOfClass:[NSArray class]])
                return INVALID;
            for (NSString *name in things) {
                if (![name isKindOfClass:[NSString class]])
                    return INVALID;
                xxpc_array_append_value(out_things, nsstring_to_xpc(name));
            }
            xxpc_dictionary_set_value(out_info, types[i].okey, out_things);
            xxpc_release(out_things);
        }
    }

    return PROVISIONAL_PASS;
}

xxpc_object_t get_hello_bundles(const char *exec_name) {
    /* TODO cache */
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

        enum convert_filters_ret ret = convert_filters(plist_dict, exec_name, info);
        if (ret == FAIL) {
            continue;
        } else if (ret == INVALID) {
            NSLog(@"bad data in plist '%@' for dylib '%@'", full_plist, dylib);
            continue;
        }

        NSString *dylib_path = [base stringByAppendingPathComponent:dylib];
        xxpc_dictionary_set_value(info, "dylib", nsstring_to_xpc(dylib_path));

        xxpc_array_append_value(bundles, info);
        xxpc_release(info);
    }

    return bundles;
}

#define PRECISE objc_precise_lifetime

static bool handle_message(xxpc_object_t request, xxpc_object_t reply) {
    const char *type = xxpc_dictionary_get_string(request, "type");
    if (!type || strcmp(type, "hello"))
        return false;
    const char *argv0 = xxpc_dictionary_get_string(request, "argv0");
    if (!argv0)
        return false;
    xxpc_object_t bundles = get_hello_bundles(argv0);
    xxpc_dictionary_set_value(reply, "bundles", bundles);
    xxpc_release(bundles);
    return true;
}

static void init_peer(xxpc_object_t peer) {
    xxpc_connection_set_event_handler(peer, ^(xxpc_object_t event) {
        xxpc_type_t ty = xxpc_get_type(event);
        if (ty == XXPC_TYPE_DICTIONARY) {
            xxpc_object_t reply = xxpc_dictionary_create_reply(event);
            if (handle_message(event, reply))
                xxpc_connection_send_message(peer, reply);
            else
                xxpc_connection_cancel(peer);
            xxpc_release(reply);
        } else if (ty == XXPC_TYPE_ERROR) {
            if (event == XXPC_ERROR_CONNECTION_INTERRUPTED)
                return;
            NSLog(@"XPC error from peer: %@", event);
        } else {
            NSLog(@"unknown object received from XPC (peer): %@", event);
        }
    });
    xxpc_connection_resume(peer);
}

/* This is a daemon contacted by all processes which can load extensions.  It
 * currently does the work of reading the plists in
 * /Library/Substitute/DynamicLibraries in order to avoid loading objc/CF
 * libraries into the target binary (unless actually required by loaded
 * libraries).  In the future it will help with hot loading. */

int main() {
    xxpc_connection_t listener = xxpc_connection_create_mach_service(
        "com.ex.substituted", NULL, XXPC_CONNECTION_MACH_SERVICE_LISTENER);
    if (!listener) {
        NSLog(@"xxpc_connection_create_mach_service returned null");
        exit(1);
    }
    xxpc_connection_set_event_handler(listener, ^(xxpc_object_t object) {
        xxpc_type_t ty = xxpc_get_type(object);
        if (ty == XXPC_TYPE_CONNECTION) {
            init_peer(object);
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
