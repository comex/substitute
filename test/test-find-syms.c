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
	substitute_sym *syms[1];
	assert(!substitute_find_private_syms(im, names, syms, 1));
	assert(syms[0]);

	int (*f)(int) = (int (*)(int)) substitute_sym_to_ptr(im, syms[0]);
	assert(f(12345) < 0);

	substitute_close_image(im);
}
