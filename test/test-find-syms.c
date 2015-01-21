#include <substitute.h>
#include <stdio.h>
#include <assert.h>
#include <dlfcn.h>

int main() {
	const char *foundation = "/System/Library/Frameworks/Foundation.framework/Foundation";
	dlopen(foundation, RTLD_LAZY);
	struct substitute_image *im = substitute_open_image(foundation);
	assert(im);
	const char *names[] = { "_absolute_from_gregorian" };
	int (*f)(int);
	assert(!substitute_find_private_syms(im, names, (void **) &f, 1));
	assert(f);

	assert(f(12345) < 0);

	substitute_close_image(im);
}
