#define WANT_BSDTHREAD_TERMINATE
#define WANT_SEMAPHORE_WAIT_TRAP
#include "darwin/manual-syscall.h"

#ifdef __arm64__
#define _PAGE_SIZE 0x4000
#else
#define _PAGE_SIZE 0x1000
#endif

int manual_bsdthread_terminate(void *, unsigned long, unsigned, unsigned);
GEN_SYSCALL(bsdthread_terminate, 361);
int manual_semaphore_wait_trap(int);
GEN_SYSCALL(semaphore_wait_trap, -36);

/* This is somewhat more complicated than it has to be because it does not use
 * pthread_join, which depends on pthread_self, which would need to be
 * initialized manually; the format of this has changed in the past, and could
 * again. */

struct baton {
    int (*pthread_create)(int *, void *, void *(*)(void *), void *);
    int (*pthread_detach)(int);
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
    int pt = 0;
    baton->pthread_create(&pt, 0, (void *) bsd_thread_func, baton);
    baton->pthread_detach(pt);
    manual_bsdthread_terminate(0, 0, 0, baton->sem_port);
    ((void (*)()) 0xbad)();
}
static int bsd_thread_func(void *arg) {
    struct baton *baton = arg;
    void *r = baton->dlopen(baton->path, 0);
    if (r) {
        __attribute__((section("__TEXT,__text"), aligned(4)))
        static char name[] = "substitute_init";
        void (*init)(void *, unsigned long) = baton->dlsym(r, name);
        if (init)
            init(baton->shuttle, baton->nshuttle);

    }
    manual_semaphore_wait_trap(baton->sem_port);
#ifndef __i386__
    /* since we're munmapping our own code, this must be optimized into a jump
     * (tail call elimination) */
    enum { page_size =
#ifdef __arm64__
            0x4000
#else
            0x1000
#endif
    };
    unsigned long ptr = (unsigned long) baton & ~(_PAGE_SIZE - 1);
    return baton->munmap((void *) ptr, 2 * page_size);
#else
    /* i386 can't normally eliminate tail calls in caller-cleanup calling
     * conventions, unless the number of arguments is the same, so use a nasty
     * hack */
    extern int jump_to_munmap(void *);
    return jump_to_munmap(baton);
#endif
}
#ifdef __i386__
/* yuck */
asm(".globl _jump_to_munmap;"
    "_jump_to_munmap:"
        "push %ebp;"
        "mov %esp, %ebp;"
        "sub $0x400, %ebp;"
        "mov 8(%esp), %edx;" /* baton */
        "mov 16(%edx), %eax;" /* munmap */
        "and $~0xfff, %edx;"
        "mov %edx, 8(%ebp);"
        "movl $0x2000, 12(%ebp);"
        "add $3, %eax;" /* !? */
        "jmp *%eax;"
);
#endif
