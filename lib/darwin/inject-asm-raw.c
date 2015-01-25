#include "darwin/manual-syscall.h"

#ifdef __arm64__
#define _PAGE_SIZE 0x4000
#else
#define _PAGE_SIZE 0x1000
#endif

struct baton {
    int (*pthread_create)(int *, void *, void *(*)(void *), void *);
    void (*dlopen)(const char *, int);
    const char *path;
    long done;
};
struct baton2 {
    void (*dlopen)(const char *, int);
    const char *path;
    int port;
};
static void *bsd_thread_func(void *);
#if defined(__i386__)
__attribute__((fastcall))
#endif
/* xxx need to change this to have host allocate two pages - way easier */
void entry(struct baton *baton) {
    int pt;
    baton->pthread_create(&pt, 0, bsd_thread_func, baton);
    unsigned long ptr = (unsigned long) baton & ~(_PAGE_SIZE - 1);
    while (!baton->done)
        manual_syscall(-62 /*clock_sleep_trap */, 0, 1, 0, 8000 /*ns*/, -1);
    manual_syscall(361 /*bsdthread_terminate*/, ptr, 0x2000, 0, 0, 0);
    ((void (*)()) 0xbad)();
}
static void *bsd_thread_func(void *arg) {
    struct baton *baton = arg;
    baton->dlopen(baton->path, 0);
    baton->done = 1;
    return 0;
}
