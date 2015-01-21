#ifdef __arm64__
#define _PAGE_SIZE 0x4000
#else
#define _PAGE_SIZE 0x1000
#endif

#define REG(var, reg) register long _##var asm(#reg) = var
#define OREG(var, reg) register long var asm(#reg)

__attribute__((always_inline))
#if defined(__x86_64__)
static int syscall(long s, long a, long b, long c, long d, long e) {
    if (s < 0)
        s = -s | 1 << 24;
    else
        s |= 2 << 24;
    REG(s, rax); REG(a, rdi); REG(b, rsi); REG(c, rdx); REG(d, rcx);
    OREG(out, rax);
    asm volatile("push %1; syscall; pop %1"
                 : "=r"(out)
                 : "r"(e), "r"(_s), "r"(_a), "r"(_b), "r"(_c), "r"(_d));
    return out;
}
#elif defined(__i386__)
static int syscall(long s, long a, long b, long c, long d, long e) {
    REG(s, eax);
    OREG(out, eax);
    OREG(sp, ecx);
    asm volatile("mov %%esp, %0" : "=r"(sp));
    asm volatile("push %0" :: "r"(e));
    asm volatile("push %0" :: "r"(d));
    asm volatile("push %0" :: "r"(c));
    asm volatile("push %0" :: "r"(b));
    asm volatile("push %0" :: "r"(a));
    asm volatile("call 1f; 1: pop %%edx; add $(2f-1b), %%edx;"
                 "sysenter; 2:"
        : "=r"(out) : "r"(_s) : "edx");
    return out;
}
#elif defined(__arm__)
static int syscall(long s, long a, long b, long c, long d, long e) {
    REG(s, r12); REG(a, r0); REG(b, r1); REG(c, r2); REG(d, r3);
    OREG(out, r0);
    asm volatile("push {%1}; svc #0x80; pop {%1}"
                 : "=r"(out)
                 : "r"(e), "r"(_s), "r"(_a), "r"(_b), "r"(_c), "r"(_d));
    return out;
}
#elif defined(__arm64__)
static int syscall(long s, long a, long b, long c, long d, long e) {
    REG(s, x16); REG(a, x0); REG(b, x1); REG(c, x2); REG(d, x3);
    OREG(out, x0);
    asm volatile("str %1, [sp, #-0x10]!\n svc #0x80\n ldr %1, [sp], #0x10"
                 : "=r"(out)
                 : "r"(e), "r"(_s), "r"(_a), "r"(_b), "r"(_c), "r"(_d));
    return out;
}
#else
#error ?
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
        syscall(-62 /*clock_sleep_trap */, 0, 1, 0, 8000 /*ns*/, -1);
    syscall(361 /*bsdthread_terminate*/, ptr, 0x2000, 0, 0, 0);
    ((void (*)()) 0xbad)();
}
static void *bsd_thread_func(void *arg) {
    struct baton *baton = arg;
    baton->dlopen(baton->path, 0);
    baton->done = 1;
    return 0;
}
