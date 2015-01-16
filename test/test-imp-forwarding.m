#include "../lib/objc.c"
#include <objc/runtime.h>
#include <stdio.h>

static void imp1(id self, SEL sel, int a, int b) {
    printf("imp1: self=%p sel=%s a=%d b=%d\n", self, sel_getName(sel), a, b);
}

struct big {
    int x[10];
};

static struct big imp2(id self, SEL sel, int a, int b) {
    printf("imp2: self=%p sel=%s a=%d b=%d\n", self, sel_getName(sel), a, b);
    return (struct big) {{0}};
}

int main() {
    IMP old = (IMP) imp1;
    SEL sel = @selector(some);
    struct temp_block_literal temp_block = get_temp_block(&old, sel);
    IMP new = imp_implementationWithBlock((id) &temp_block);
    ((void (*)(id, SEL, int, int)) new)(@"foo", sel, 1, 2);
    old = (IMP) imp2;
    ((struct big (*)(id, SEL, int, int)) new)(@"bar", sel, 1, 2);
}
