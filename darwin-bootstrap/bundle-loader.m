#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
extern char ***_NSGetArgv(void);

#define PREFIX "Substitute bundle loader: "

enum test_filters_ret {
    PASSED,
    FAILED,
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

static enum test_filters_ret test_filters(NSDictionary *plist_dict) {

    NSDictionary *filter = [plist_dict objectForKey:@"Filter"];
    if (!filter)
        return PASSED;
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
            return FAILED;
        id supremum_o = [cfv objectAtIndex:1];
        if (supremum_o) {
            double supremum = id_to_double(supremum_o);
            if (supremum != supremum)
                return INVALID;
            if (version >= supremum)
                return FAILED;
        }
    }

    NSArray *classes = [filter objectForKey:@"Classes"];
    if (classes) {
        if (![classes isKindOfClass:[NSArray class]])
            return INVALID;
        for (NSString *name in classes) {
            if (![name isKindOfClass:[NSString class]])
                return INVALID;
            if (NSClassFromString(name))
                goto ok1;
        }
        return FAILED;
    ok1:;
    }

    NSArray *bundles = [filter objectForKey:@"Bundles"];
    if (bundles) {
        if (![bundles isKindOfClass:[NSArray class]])
            return INVALID;
        for (NSString *identifier in bundles) {
            if (![identifier isKindOfClass:[NSString class]])
                return INVALID;
            if ([NSBundle bundleWithIdentifier:identifier])
                goto ok2;
        }
        return FAILED;
    ok2:;
    }


    NSArray *executables = [filter objectForKey:@"Executables"];
    if (executables) {
        const char *argv0 = (*_NSGetArgv())[0];
        NSString *exe = nil;
        if (argv0) {
            NSString *nsargv0 = [NSString stringWithCString:argv0
                                          encoding:NSUTF8StringEncoding];
            exe = [[nsargv0 pathComponents] lastObject];
        }
        if (!exe)
            exe = @"";
        if (![executables isKindOfClass:[NSArray class]])
            return INVALID;
        for (NSString *name in executables) {
            if (![name isKindOfClass:[NSString class]])
                return INVALID;
            if ([name isEqualToString:exe])
                goto ok3;
        }
        return FAILED;
    ok3:;
    }
    return PASSED;
}

/* this is DYLD_INSERT_LIBRARIES'd, not injected. */
__attribute__((constructor))
static void init() {
    NSError *err;
    NSString *base = @"/Library/Substitute/DynamicLibraries";
    NSArray *list = [[NSFileManager defaultManager]
                     contentsOfDirectoryAtPath:base
                     error:&err];
    if (!list)
        return;

    for (NSString *dylib in list) {
        if (![[dylib pathExtension] isEqualToString:@"dylib"])
            continue;
        NSString *plist = [[dylib stringByDeletingPathExtension]
                           stringByAppendingPathExtension:@"plist"];
        NSString *full_plist = [base stringByAppendingPathComponent:plist];
        NSDictionary *plist_dict = [NSDictionary dictionaryWithContentsOfFile:
                                    full_plist];
        if (!plist_dict) {
            NSLog(@PREFIX "missing, unreadable, or invalid plist '%@' for dylib '%@'; unlike Substrate, we require plists", full_plist, dylib);
            continue;
        }
        enum test_filters_ret ret = test_filters(plist_dict);
        if (ret == FAILED) {
            continue;
        } else if (ret == INVALID) {
            NSLog(@PREFIX "bad data in plist '%@' for dylib '%@'", full_plist, dylib);
            continue;
        }
        NSString *full_dylib = [base stringByAppendingPathComponent:dylib];
        const char *c_dylib = [full_dylib cStringUsingEncoding:NSUTF8StringEncoding];
        if (!c_dylib) {
            NSLog(@PREFIX "Not loading weird dylib path %@", full_dylib);
            continue;
        }
        NSLog(@"Substitute loading %@", full_dylib);
        if (!dlopen(c_dylib, RTLD_LAZY)) {
            NSLog(@PREFIX "Failed to load %@: %s", full_dylib, dlerror());
        }
    }
}
