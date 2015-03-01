#include "substitute.h"
#include "substitute-internal.h"
#include <stdio.h>
#include <search.h>
#include <unistd.h>
#include <errno.h>
static pid_t (*old_getpid)();
static pid_t hook_getpid() {
    return old_getpid() * 2;
}

static int hook_hcreate(size_t nel) {
    return (int) nel;
}

static size_t (*old_fwrite)(const void *restrict, size_t, size_t, FILE *restrict);
static size_t hook_fwrite(const void *restrict ptr, size_t size, size_t nitems,
                          FILE *restrict stream) {
    size_t ret = old_fwrite(ptr, size, nitems, stream);
    old_fwrite("*hic*\n", 1, 6, stream);
    return ret;
}

__attribute__((noinline))
void break_before() {
    __asm__ volatile("");
}

__attribute__((noinline))
void break_after() {
    __asm__ volatile("");
}

__attribute__((section("__TEST,__foo"), noinline))
static int my_own_function(int x) {
    return x + 4;
}

static int hook_my_own_function(int x) {
    return x + 5;
}

static const struct substitute_function_hook hooks[] = {
    {my_own_function, hook_my_own_function, NULL},
    {getpid, hook_getpid, &old_getpid},
    {hcreate, hook_hcreate, NULL},
    {fwrite, hook_fwrite, &old_fwrite},
};
int main() {
#ifdef TARGET_DIS_SUPPORTED
    for (size_t i = 0; i < sizeof(hooks)/sizeof(*hooks); i++) {
        uintptr_t p = (uintptr_t) hooks[i].function;
        uint32_t *insns = (void *) (p & ~1);
        printf("<%zd: ptr=%p insns=0x%08x, 0x%08x, 0x%08x\n",
               i, hooks[i].function,
               insns[0], insns[1], insns[2]);

    }
    printf("getpid() => %d\n", getpid());
    break_before();
    int ret = substitute_hook_functions(hooks, sizeof(hooks)/sizeof(*hooks),
                                        NULL, 0);
    break_after();
    int e = errno;
    printf("ret = %d\n", ret);
    printf("errno = %d\n", e);
    printf("getpid() => %d\n", getpid());
    printf("hcreate() => %d\n", hcreate(42));
    printf("my_own_function() => %d\n", my_own_function(0));
#else
    (void) hooks;
    printf("can't test this here\n");
#endif
}
