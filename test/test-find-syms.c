#include <substitute.h>
#include <stdio.h>
#include <assert.h>
#include <dlfcn.h>

int main() {
	const char *foundation = "/System/Library/Frameworks/Foundation.framework/Foundation";
	dlopen(foundation, RTLD_LAZY);
	const char *names[] = { "_setshortValueWithMethod" };
	substitute_sym *syms[1];
	assert(!substitute_find_syms(foundation, names, syms, 1));
	printf("%p\n", syms[0]);
}
