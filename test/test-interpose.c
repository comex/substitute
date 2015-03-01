#include "substitute.h"

#include <unistd.h>
#include <stdio.h>
#include <mach-o/dyld.h>
#include <assert.h>

static void *getpid_plus = (void *) getpid + 5;

static pid_t (*old_getpid)();
static pid_t my_getpid() {
	return old_getpid() + 1;
}
static gid_t my_getgid() {
	return 42;
}

int main() {
	const char *self = _dyld_get_image_name(0);
	void *handle = substitute_open_image(self);
	assert(handle);
	struct substitute_import_hook hooks[] = {
		{"_getpid", my_getpid, &old_getpid},
		{"_getgid", my_getgid, NULL},

	};
	pid_t (*gp)() = getpid_plus - 5;
	printf("original pid: %d\n", (int) gp());
	substitute_interpose_imports(handle, hooks, sizeof(hooks)/sizeof(*hooks), NULL, 0);
	gp = getpid_plus - 5;
	printf("new pid: %d\n", (int) gp());
	printf("new gid: %d\n", (int) getgid());

	substitute_close_image(handle);
}
