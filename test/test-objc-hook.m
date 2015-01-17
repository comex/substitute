#include "substitute.h"
#import <Foundation/Foundation.h>

@interface Base : NSObject {
}
- (void)foo:(NSString *)str;
- (void)bar:(NSString *)str;
@end

@implementation Base
- (void)foo:(NSString *)str {
    NSLog(@"base foo: %@", str);
}
- (void)bar:(NSString *)str {
    NSLog(@"base bar: %@", str);
}
@end

@interface Derived : Base {
}
@end

@implementation Derived
- (void)foo:(NSString *)str {
    NSLog(@"derived foo: %@", str);
}
@end

static void (*old_foo)(id, SEL, NSString *);
static void new_foo(id self, SEL sel, NSString *str) {
    NSLog(@"new foo: %@; calling orig", str);
    return old_foo(self, sel, str);
}
static void (*old_bar)(id, SEL, NSString *);
static void new_bar(id self, SEL sel, NSString *str) {
    NSLog(@"new bar: %@; calling orig", str);
    return old_bar(self, sel, str);
}

int main() {
    assert(!substitute_hook_objc_message([Derived class], @selector(foo:), new_foo, &old_foo, NULL));
    assert(!substitute_hook_objc_message([Derived class], @selector(bar:), new_bar, &old_bar, NULL));
    Derived *d = [[Derived alloc] init];
    [d foo:@"hi!"];
    [d bar:@"hello!"];
}
