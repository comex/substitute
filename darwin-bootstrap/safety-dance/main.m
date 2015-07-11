#import <UIKit/UIKit.h>
#import "AutoGrid.h"

@interface ViewController : UIViewController {
    AutoGrid *autoGrid;
}

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    [self loadStuff];
    NSMutableArray *names = [NSMutableArray array];
    for (int i = 0; i < 200; i++)
        [names addObject:[NSString stringWithFormat:@"Some Dylib %d", i]];
    NSMutableArray *views = [NSMutableArray array];
    for (NSString *name in names) {
        UILabel *label = [[UILabel alloc] init];
        label.text = name;
        [views addObject:label];
    }
    [autoGrid setViews:views];
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

    UIButton *continueButton = [UIButton buttonWithType:UIButtonTypeSystem];
    continueButton.translatesAutoresizingMaskIntoConstraints = NO;
    hugging(continueButton, 999);
    compression(continueButton, 300);
    continueButton.contentVerticalAlignment = UIControlContentVerticalAlignmentCenter;
    continueButton.contentHorizontalAlignment = UIControlContentHorizontalAlignmentCenter;
    continueButton.titleLabel.font = [UIFont systemFontOfSize:17];
    [continueButton setTitle:@"Continue in Safe Mode" forState:UIControlStateNormal];
    [self.view addSubview:continueButton];

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
    NSLog(@"dflwo");
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    ViewController *viewController = [[ViewController alloc] init];
    self.window.rootViewController = viewController;
    [self.window makeKeyAndVisible];
    return YES;
}

@end

int main(int argc, char *argv[]) {
    NSLog(@"main");
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, @"AppDelegate");
    }
}

