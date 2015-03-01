#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#include "substituted-messages.h"

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

static enum convert_filters_ret convert_filters(NSDictionary *plist_dict,
                                                const char *exec_name,
                                                NSMutableData *test_out) {

    NSDictionary *filter = [plist_dict objectForKey:@"Filter"];
    if (!filter)
        return PROVISIONAL_PASS;
    if (![filter isKindOfClass:[NSDictionary class]])
        return INVALID;

    for (NSString *key in [filter allKeys]) {
        if (!([key isEqualToString:@"CoreFoundationVersion"] ||
              [key isEqualToString:@"Classes"] ||
              [key isEqualToString:@"Bundles"] ||
              [key isEqualToString:@"Executables"])) {
            return INVALID;
        }
    }

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
        return FAIL;
    ok:;
    }

    /* Convert the rest to substituted_bundle_list_ops. */

    struct {
        __unsafe_unretained NSString *key;
        uint8_t opc;
    } types[2] = {
        {@"Classes", SUBSTITUTED_TEST_CLASS},
        {@"Bundles", SUBSTITUTED_TEST_BUNDLE},
    };

    for (int i = 0; i < 2; i++) {
        NSArray *things = [filter objectForKey:types[i].key];
        if (things) {
            if (![things isKindOfClass:[NSArray class]])
                return INVALID;
            for (NSString *name in things) {
                if (![name isKindOfClass:[NSString class]])
                    return INVALID;
                NSData *name_data = [name dataUsingEncoding:NSUTF8StringEncoding];
                size_t len = [name_data length];
                if (len > 65535)
                    return INVALID;
                struct substituted_bundle_list_op op = {(uint16_t) len,
                                                        types[i].opc};
                [test_out appendBytes:&op length:sizeof(op)];
                [test_out appendData:name_data];
                static char zero = '\0';
                [test_out appendBytes:&zero length:1];
            }
        }
    }

    return PROVISIONAL_PASS;
}

static NSData *the_bundle_list;

void get_bundle_list(const char *exec_name,
                     const void **bundle_list, size_t *bundle_list_size) {
    /* TODO cache */
    NSError *err;
    NSString *base = @"/Library/Substitute/DynamicLibraries";
    NSArray *list = [[NSFileManager defaultManager]
                     contentsOfDirectoryAtPath:base
                     error:&err];
    if (!list)
        return;

    NSMutableData *out = [NSMutableData data];

    for (NSString *dylib in list) {
        if (![[dylib pathExtension] isEqualToString:@"dylib"])
            continue;
        NSString *plist = [[dylib stringByDeletingPathExtension]
                           stringByAppendingPathExtension:@"plist"];
        NSString *full_plist = [base stringByAppendingPathComponent:plist];
        NSDictionary *plist_dict = [NSDictionary dictionaryWithContentsOfFile:
                                    full_plist];
        if (!plist_dict) {
            NSLog(@"missing, unreadable, or invalid plist '%@' for dylib '%@'; unlike Substrate, we require plists", full_plist, dylib);
            continue;
        }

        NSMutableData *test = [NSMutableData data];
        enum convert_filters_ret ret = convert_filters(plist_dict, exec_name, test);
        if (ret == FAIL) {
            continue;
        } else if (ret == INVALID) {
            NSLog(@"bad data in plist '%@' for dylib '%@'", full_plist, dylib);
            continue;
        }
        NSString *dylib_path = [base stringByAppendingPathComponent:dylib];
        NSData *dylib_path_data = [dylib_path dataUsingEncoding:NSUTF8StringEncoding];
        size_t len = [dylib_path length];
        if (len > 65535) {
            NSLog(@"dylib '%@' somehow has an absurdly long path", dylib);
            continue;
        }
        [out appendData:test];
        struct substituted_bundle_list_op op = {(uint16_t) len,
                                                SUBSTITUTED_USE_DYLIB};
        [out appendBytes:&op length:sizeof(op)];
        [out appendData:dylib_path_data];
        static char zero = '\0';
        [out appendBytes:&zero length:1];
    }
    the_bundle_list = out;
    *bundle_list = [out bytes];
    *bundle_list_size = [out length];
}
