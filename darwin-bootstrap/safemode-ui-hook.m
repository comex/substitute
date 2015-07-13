#include "substitute.h"
#include <objc/runtime.h>
#include <notify.h>
#include <dispatch/dispatch.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include <mach-o/dyld.h>

static Class SpringBoard, SBApplicationController;
static bool g_did_activate_safetydance;

@interface _SBApplicationController
- (id)applicationWithBundleIdentifier:(NSString *)identifier;
+ (instancetype)sharedInstanceIfExists;
@end

@interface _SpringBoard
- (void)relaunchSpringBoard;
@end

@interface _SBApplication
- (void)setFlag:(int64_t)flag forActivationSetting:(unsigned)setting;
- (BOOL)launchApplicationWithIdentifier:(NSString *)iden
        suspended:(BOOL)suspended;
- (BOOL)isRunning;
- (void)activate;
@end

static bool detect_substrate_safe_mode() {
    /* Defer to Substrate's safe mode.  On my device, this doesn't seem to be
     * strictly necessary, because for whatever reason SafetyDance doesn't get
     * launched when MobileSafety is loaded, but let's not rely on that
     * quirk... */
    for (uint32_t i = 0, count = _dyld_image_count();
         i < count; i++) {
        const char *name = _dyld_get_image_name(i);
        if (name && strstr(name, "MobileSafety"))
            return true;
    }
    return false;
}

void (*old_applicationDidFinishLaunching)(id, SEL, id);
static void my_applicationDidFinishLaunching(id self, SEL sel, id app) {
    old_applicationDidFinishLaunching(self, sel, app);
    if (detect_substrate_safe_mode()) {
        NSLog(@"Deferring to Substrate's safe mode.");
        return;
    }
    id controller = [SBApplicationController sharedInstanceIfExists];
    if (!controller) {
        NSLog(@"substitute safe mode: sharedInstanceIfExists => nil!");
        return;
    }
    NSString *bundle_id = @"com.ex.SafetyDance";
    id sbapp = [controller applicationWithBundleIdentifier:bundle_id];
    if (!sbapp) {
        NSLog(@"substitute safe mode: no app with bundle ID '%@' - installation messed up?",
              bundle_id);
        return;
    }
    [sbapp setFlag:1 forActivationSetting:1]; /* noAnimate */
    /* [sbapp setFlag:1 forActivationSetting:5]; */ /* seo */
    [self launchApplicationWithIdentifier:bundle_id suspended:NO];
    g_did_activate_safetydance = true;
}

BOOL (*old_handleDoubleHeightStatusBarTap)(id, SEL, int64_t);
static BOOL my_handleDoubleHeightStatusBarTap(id self, SEL sel, int64_t number) {
    if (g_did_activate_safetydance && number == 202) {
        NSString *bundle_id = @"com.ex.SafetyDance";
        id controller = [SBApplicationController sharedInstanceIfExists];
        id sbapp = [controller applicationWithBundleIdentifier:bundle_id];
        if ([sbapp isRunning]) {
            NSLog(@"activate!");
            [sbapp setFlag:1 forActivationSetting:20]; /* fromBanner */
            [self launchApplicationWithIdentifier:bundle_id suspended:NO];
            return YES;
        }
    }
    return old_handleDoubleHeightStatusBarTap(self, sel, number);
}

__attribute__((constructor))
static void init() {
    #define GET(clsname) \
        clsname = objc_getClass(#clsname); \
        if (!clsname) { \
            NSLog(@"substitute safe mode failed to find %s", #clsname); \
            return; \
        }

    GET(SpringBoard);
    GET(SBApplicationController);

    int notify_token;
    uint32_t notify_status = notify_register_dispatch(
        "com.ex.substitute.safemode-restart-springboard-plz",
        &notify_token, dispatch_get_main_queue(), ^(int tok) {
            id sb = [UIApplication sharedApplication];
            [sb relaunchSpringBoard];
        }
    );

    #define HOOK(cls, sel, selvar) do { \
        int ret = substitute_hook_objc_message(cls, @selector(sel), \
                                               my_##selvar, \
                                               &old_##selvar, NULL); \
        if (ret) { \
            NSLog(@"substitute safe mode '%s' hook failed: %d", #sel, ret); \
            return; \
        } \
    } while(0)

    HOOK(SpringBoard, applicationDidFinishLaunching:,
         applicationDidFinishLaunching);
    HOOK(SpringBoard, handleDoubleHeightStatusBarTap:,
         handleDoubleHeightStatusBarTap);
}
