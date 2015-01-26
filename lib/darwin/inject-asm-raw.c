#include "darwin/manual-syscall.h"

#ifdef __arm64__
#define _PAGE_SIZE 0x4000
#else
#define _PAGE_SIZE 0x1000
#endif

struct baton {
    int (*pthread_create)(int *, void *, void *(*)(void *), void *);
    void *(*dlopen)(const char *, int);
    void *(*dlsym)(void *, const char *);
    int (*munmap)(void *, long);
    const char *path;
    /* bsd_thread_func uses this to wait for entry to go away */
    long sem_port;
    long nshuttle;
    char shuttle[0];
};
static int bsd_thread_func(void *);
#if defined(__i386__)
__attribute__((fastcall))
#endif
void entry(struct baton *baton) {
    int pt;
    baton->pthread_create(&pt, 0, (void *) bsd_thread_func, baton);
    manual_syscall(361 /* bsdthread_terminate */, 0, 0, 0, baton->sem_port);
    ((void (*)()) 0xbad)();
}
static int bsd_thread_func(void *arg) {
    struct baton *baton = arg;
    void *r = baton->dlopen(baton->path, 0);
    if (r) {
        __attribute__((section("__TEXT,__text")))
        static char name[] = "substitute_init";
        void (*init)(void *, unsigned long) = baton->dlsym(r, name);
        if (init)
            init(baton->shuttle, baton->nshuttle);

    }
    manual_syscall(-36 /* semaphore_wait_trap */, baton->sem_port, 0, 0, 0);
    unsigned long ptr = (unsigned long) baton & ~(_PAGE_SIZE - 1);
    return baton->munmap((void *) ptr, 0x2000);
}
