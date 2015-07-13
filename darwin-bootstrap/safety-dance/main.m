#include "darwin/xxpc.h"
#include "substitute-internal.h"
#import <UIKit/UIKit.h>
#import "AutoGrid.h"
#include <notify.h>

static NSArray *g_dylibs_to_show;

@interface UIApplication (Private)
- (void)terminateWithSuccess;
- (void)suspend;

- (void)setGlowAnimationEnabled:(BOOL)enabled forStyle:(int)style;
- (void)addStatusBarStyleOverrides:(int)overrides;
- (void)removeStatusBarStyleOverrides:(int)overrides;
- (void)setDoubleHeightStatusText:(NSString *)text forStyle:(int)style;
@end

@interface ViewController : UIViewController {
    AutoGrid *autoGrid;
}

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    [self loadStuff];
    /*
    NSMutableArray *names = [NSMutableArray array];
    for (int i = 0; i < 200; i++)
        [names addObject:[NSString stringWithFormat:@"Some Dylib %d", i]];
    g_dylibs_to_show = names;
    */
    NSMutableArray *views = [NSMutableArray array];
    for (NSString *name in g_dylibs_to_show) {
        UILabel *label = [[UILabel alloc] init];
        label.text = name;
        [views addObject:label];
    }
    [autoGrid setViews:views];
}

- (void)exitReturnToNormal {
    notify_post("com.ex.substitute.safemode-restart-springboard-plz");
}

- (void)exitContinueSafe {
    /* mimic Setup.app, but we don't want to actually quit */
    UIApplication *app = [UIApplication sharedApplication];
    [app suspend];
}

#define EXPLANATION \
    @"SpringBoard seems to have crashed.  This might have been caused by Substitute jailbreak extension, or it could be unrelated.  Just to be safe, extensions in SpringBoard have been temporarily disabled.\n\nThe following extensions were running:"

static void hugging(UIView *view, UILayoutPriority pri) {
    [view setContentHuggingPriority:pri forAxis:UILayoutConstraintAxisHorizontal];
    [view setContentHuggingPriority:pri forAxis:UILayoutConstraintAxisVertical];
}
static void compression(UIView *view, UILayoutPriority pri) {
    [view setContentCompressionResistancePriority:pri forAxis:UILayoutConstraintAxisHorizontal];
    [view setContentCompressionResistancePriority:pri forAxis:UILayoutConstraintAxisVertical];
}

- (void)loadStuff {
    self.view.backgroundColor = [UIColor whiteColor];

    UILabel *top = [[UILabel alloc] init];
    top.translatesAutoresizingMaskIntoConstraints = NO;
    top.textAlignment = NSTextAlignmentCenter;
    hugging(top, 251);
    top.text = @"libsubstitute";
    top.font = [UIFont systemFontOfSize:23];
    [self.view addSubview:top];

    UILabel *big = [[UILabel alloc] init];
    big.translatesAutoresizingMaskIntoConstraints = NO;
    big.textAlignment = NSTextAlignmentCenter;
    hugging(big, 251);
    [big setContentHuggingPriority:251 forAxis:UILayoutConstraintAxisHorizontal];
    [big setContentHuggingPriority:251 forAxis:UILayoutConstraintAxisVertical];
    big.text = @"Safe Mode";
    big.font = [UIFont systemFontOfSize:32];
    [self.view addSubview:big];

    UILabel *explain = [[UILabel alloc] init];
    explain.translatesAutoresizingMaskIntoConstraints = NO;
    explain.textAlignment = NSTextAlignmentCenter;
    hugging(explain, 251);
    compression(explain, 999);
    explain.text = EXPLANATION;
    explain.font = [UIFont systemFontOfSize:14];
    explain.minimumScaleFactor = 0.5; /* test */
    explain.numberOfLines = 0;
    [self.view addSubview:explain];

    UIButton *returnButton = [UIButton buttonWithType:UIButtonTypeSystem];
    returnButton.translatesAutoresizingMaskIntoConstraints = NO;
    returnButton.contentVerticalAlignment = UIControlContentVerticalAlignmentCenter;
    returnButton.contentHorizontalAlignment = UIControlContentHorizontalAlignmentCenter;
    returnButton.titleLabel.font = [UIFont systemFontOfSize:17];
    [returnButton setTitle:@"Return to Normal" forState:UIControlStateNormal];
    [self.view addSubview:returnButton];
    [returnButton addTarget:self action:@selector(exitReturnToNormal)
                  forControlEvents:UIControlEventTouchUpInside];

    UIButton *continueButton = [UIButton buttonWithType:UIButtonTypeSystem];
    continueButton.translatesAutoresizingMaskIntoConstraints = NO;
    hugging(continueButton, 999);
    compression(continueButton, 300);
    continueButton.contentVerticalAlignment = UIControlContentVerticalAlignmentCenter;
    continueButton.contentHorizontalAlignment = UIControlContentHorizontalAlignmentCenter;
    continueButton.titleLabel.font = [UIFont systemFontOfSize:17];
    [continueButton setTitle:@"Continue in Safe Mode" forState:UIControlStateNormal];
    [self.view addSubview:continueButton];
    [continueButton addTarget:self action:@selector(exitContinueSafe)
                    forControlEvents:UIControlEventTouchUpInside];

    autoGrid = [[AutoGrid alloc] init];
    autoGrid.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:autoGrid];

    NSDictionary *viewsDictionary = @{
        @"top": top,
        @"big": big,
        @"explain": explain,
        @"returnButton": returnButton,
        @"continueButton": continueButton,
        @"grid": autoGrid,
        @"topGuide": self.topLayoutGuide,
        @"bottomGuide": self.bottomLayoutGuide,
    };
    NSMutableArray *constraints = [[NSMutableArray alloc] init];
    [constraints addObjectsFromArray:
        [NSLayoutConstraint constraintsWithVisualFormat:
            @"V:[topGuide]-10-[top]-0@100-[big]-0@100-[explain]-18@200-[grid]-18-[continueButton]-8-[returnButton]-20@100-[bottomGuide]"
            options:NSLayoutFormatAlignAllCenterX metrics:nil views:viewsDictionary]];
    NSArray *additional = @[
        @"[explain(<=650)]",
        @"|->=10-[explain]->=10-|",
        @"|-20-[grid]-20-|",
    ];
    for (NSString *fmt in additional) {
        [constraints addObjectsFromArray:
            [NSLayoutConstraint constraintsWithVisualFormat:fmt options:0 metrics:nil views:viewsDictionary]];
    }
    [self.view addConstraints:constraints];
}


- (UIInterfaceOrientationMask)supportedInterfaceOrientations
{
    if ([UIDevice currentDevice].userInterfaceIdiom == UIUserInterfaceIdiomPad)
        return UIInterfaceOrientationMaskAll;
    else if ([UIScreen mainScreen].bounds.size.width >= 414)
        return UIInterfaceOrientationMaskAllButUpsideDown;
    else
        return UIInterfaceOrientationMaskPortrait;
}

@end

@interface AppDelegate : UIResponder <UIApplicationDelegate> {
}

@property (strong, nonatomic) UIWindow *window;


@end

@implementation AppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    ViewController *viewController = [[ViewController alloc] init];
    self.window.rootViewController = viewController;
    [self.window makeKeyAndVisible];

    return YES;
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
    /* mimic Voice Memos... */
    NSLog(@"did enter background");
    [application setGlowAnimationEnabled:YES forStyle:202];
    [application setDoubleHeightStatusText:@"Safe Mode" forStyle:202];
    [application addStatusBarStyleOverrides:4];
    NSLog(@"(done)");
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
    NSLog(@"did become active");
    [application setGlowAnimationEnabled:NO forStyle:202];
    [application removeStatusBarStyleOverrides:4];
    NSLog(@"(done)");
}

@end

static const char *test_and_transform_id_dylib(const char *id_dylib) {
    const char *base = xbasename(id_dylib);
    static const char dir1[] = "/Library/MobileSubstrate/DynamicLibraries/";
    static const char dir2[] = "/Library/Substitute/DynamicLibraries/";
    if (!strncmp(id_dylib, dir1, sizeof(dir1) - 1) ||
        !strncmp(id_dylib, dir2, sizeof(dir2) - 1))
        return base;
    char *fn = NULL, *fn2 = NULL;
    asprintf(&fn, "%s%s", dir1, base);
    asprintf(&fn2, "%s%s", dir2, base);
    bool found = !access(fn, F_OK) || !access(fn2, F_OK);
    free(fn);
    free(fn2);
    if (found)
        return base;
    return NULL;
}

static void do_bad() {
    NSLog(@"problem asking substituted for springboard-fatal-loaded-dylibs...");
    g_dylibs_to_show = @[@"<error>"];
}

static void startup() {
    xxpc_connection_t conn = xxpc_connection_create_mach_service(
        "com.ex.substituted", NULL, 0);
    if (!conn)
        return do_bad();
    xxpc_connection_set_event_handler(conn, ^(xxpc_object_t event) {
        NSLog(@"< %@", event);
    });
    xxpc_connection_resume(conn);
    xxpc_object_t request = xxpc_dictionary_create(NULL, NULL, 0);
    xxpc_dictionary_set_string(request, "type",
                               "springboard-fatal-loaded-dylibs");
    NSLog(@"asking substituted...");
    xxpc_object_t reply = xxpc_connection_send_message_with_reply_sync(
        conn, request);
    NSLog(@"done.");
    if (!reply || xxpc_get_type(reply) != XXPC_TYPE_DICTIONARY)
        return do_bad();
    NS_VALID_UNTIL_END_OF_SCOPE
    xxpc_object_t dylibs = xxpc_dictionary_get_value(reply, "dylibs");
    if (!dylibs) {
        g_dylibs_to_show = @[@"<unknown>"];
        return;
    }
    if (xxpc_get_type(dylibs) != XXPC_TYPE_ARRAY)
        return do_bad();
    NSMutableArray *ary = [NSMutableArray array];
    for (size_t i = 0, count = xxpc_array_get_count(dylibs);
         i < count; i++) {
        const char *dylib = xxpc_array_get_string(dylibs, i);
        if (!dylib)
            return do_bad();
        const char *dylib_to_show = test_and_transform_id_dylib(dylib);
        if (dylib_to_show)
            [ary addObject:[NSString stringWithCString:dylib_to_show
                                     encoding:NSUTF8StringEncoding]];
    }
    g_dylibs_to_show = ary;
}

int main(int argc, char *argv[]) {
    @autoreleasepool {
        startup();
        return UIApplicationMain(argc, argv, nil, @"AppDelegate");
    }
}

