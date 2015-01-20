#include "../lib/darwin/objc.c"
#include <objc/runtime.h>
#include <stdio.h>
#include <assert.h>
#import <Foundation/Foundation.h>

static void *what_to_call(void *a, void *b) {
    printf("what_to_call: %p %p\n", a, b);
    return a;
}

static void imp1(id self, SEL sel, int a, int b) {
    NSLog(@"imp1: self=%@ sel=%s a=%d b=%d\n", self, sel_getName(sel), a, b);
}

struct big {
    int x[10];
};

static struct big imp2(id self, SEL sel, int a, int b) {
    NSLog(@"imp2: self=%@ sel=%s a=%d b=%d\n", self, sel_getName(sel), a, b);
    return (struct big) {{4}};
}

struct big (^test)(id, int) = ^(id self, int a) {
    NSLog(@"self=%@  a=%d", self, a);
    return (struct big) {{0}};
};

int main() {
    SEL sel = @selector(some);
    IMP imp;
    assert(!get_trampoline(what_to_call, imp1, (void *) 0x123, &imp));
    uint8_t *ip = (void *) imp;
    assert(ip[TRAMPOLINE_SIZE] == ip[0]);
    printf("imp = %p\n", imp);
    ((void (*)(id, SEL, int, int)) imp)(@"foo", sel, 1, 2);
    free_trampoline(imp);
    assert(!get_trampoline(what_to_call, imp2, (void *) 0x123, &imp));
    struct big big = ((struct big (*)(id, SEL, int, int)) imp)(@"bar", sel, 1, 2);
    printf("out? %d\n", big.x[0]);

    /* test alloc/free */
    enum { n = 10000 };
    static void *imps[n];
    for(size_t i = 0; i < n; i++) {
        assert(!get_trampoline(what_to_call, imp1, (void *) 0x123, &imps[i]));
    }
    for(size_t i = 0; i < n; i++) {
        assert(imps[i]);
        if (arc4random() & 1) {
            free_trampoline(imps[i]);
            imps[i] = NULL;
        }
    }
    for(size_t i = 0; i < n; i++) {
        if (imps[i]) {
            free_trampoline(imps[i]);
            imps[i] = NULL;
        }
    }
}
