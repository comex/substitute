#include "substitute.h"
#include <objc/runtime.h>
#include <notify.h>
#include <dispatch/dispatch.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

/* TODO: test what happens when both are required */

static bool g_did_say_in_setup_mode;
static bool g_did_exit_safety_dance;

@interface _SBApplicationController
- (id)applicationWithBundleIdentifier:(NSString *)identifier;
+ (instancetype)sharedInstanceIfExists;
@end

@interface _SBSetupManager
- (bool)updateInSetupMode;
- (bool)_setInSetupMode:(bool)inSetupMode;
- (id)applicationWithBundleIdentifier:(NSString *)identifier;
@end

@interface _SpringBoard
- (void)relaunchSpringBoard;
@end

static id (*old_setupApplication)(id, SEL);
static id my_setupApplication(id self, SEL sel) {
    if (g_did_say_in_setup_mode) {
        id app = [self applicationWithBundleIdentifier:@"com.ex.SafetyDance"];
        if (app) {
            /* definitely shouldn't be nil, given below check, but... */
            NSLog(@"=>%@", app);
            return app;
        }
    }
    return old_setupApplication(self, sel);
}

static bool (*old_updateInSetupMode)(id, SEL);
static bool my_updateInSetupMode(id self, SEL sel) {
    bool should_say = !g_did_exit_safety_dance;
    if (should_say) {
        _SBApplicationController *controller =
            [objc_getClass("SBApplicationController") sharedInstanceIfExists];
        if (!controller) {
            NSLog(@"substitute safe mode: SBApplicationController missing...");
            should_say = false;
        } else if (![controller
                     applicationWithBundleIdentifier:@"com.ex.SafetyDance"]) {
            NSLog(@"substitute safe mode: SafetyDance app missing or unregistered");
            should_say = false;
        }
    }

    if (should_say) {
        /* take priority over real setup */
        g_did_say_in_setup_mode = true;
        [self _setInSetupMode:true];
        return true;
    } else {
        g_did_say_in_setup_mode = false;
        return old_updateInSetupMode(self, sel);
    }
}

static bool (*old__handleSetupExited)(id, SEL, id);
static bool my__handleSetupExited(id self, SEL sel, id app) {
    if (g_did_say_in_setup_mode)
        g_did_exit_safety_dance = true;
    return old__handleSetupExited(self, sel, app);
}

__attribute__((constructor))
static void init() {
    #define GET(clsname) \
        Class clsname = objc_getClass(#clsname); \
        if (!clsname) { \
            NSLog(@"substitute safe mode failed to find %s", #clsname); \
            return; \
        }

    GET(SBApplicationController);
    GET(SBSetupManager);
    GET(SBWorkspace);

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

    /* note: any of these might fail, leaving the previous ones hooked */

    HOOK(SBApplicationController, setupApplication, setupApplication);
    HOOK(SBWorkspace, _handleSetupExited:, _handleSetupExited);
    HOOK(SBSetupManager, updateInSetupMode, updateInSetupMode);


}
