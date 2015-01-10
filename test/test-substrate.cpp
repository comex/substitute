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
    MSHookSymbol(f, "_absolute_from_gregorian", im);
	assert(f(12345) < 0);
    clock_t a = clock();
    MSHookSymbol(f, "_absolute_from_gregorian");
    clock_t b = clock();
    printf("%ld\n", (long) (b - a));
	assert(f(12345) < 0);
}

