#include <substrate.h>
#include <stdio.h>
#include <assert.h>
#include <dlfcn.h>
#include <time.h>

int main() {
	const char *foundation = "/System/Library/Frameworks/Foundation.framework/Foundation";
	dlopen(foundation, RTLD_LAZY);

    MSImageRef im = MSGetImageByName(foundation);
	assert(im);

    int (*f)(int);
    clock_t a = clock();
    MSHookSymbol(f, "_absolute_from_gregorian", im);
    clock_t b = clock();
    printf("init: %ld\n", (long) (b - a));
    a = clock();
    MSHookSymbol(f, "_absolute_from_gregorian", im);
    b = clock();
    printf("spec: %ld\n", (long) (b - a));
	assert(f(12345) < 0);
    a = clock();
    MSHookSymbol(f, "_absolute_from_gregorian");
    b = clock();
    printf("gen: %ld\n", (long) (b - a));
	assert(f(12345) < 0);
}

