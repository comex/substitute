#pragma once

#define REG(var, reg) register long _##var __asm__(#reg) = var
#define OREG(var, reg) register long var __asm__(#reg)

__attribute__((always_inline))
#if defined(__x86_64__)
static int manual_syscall(long s, long a, long b, long c, long d) {
    if (s < 0)
        s = -s | 1 << 24;
    else
        s |= 2 << 24;
    REG(s, rax); REG(a, rdi); REG(b, rsi); REG(c, rdx); REG(d, rcx);
    OREG(out, rax);
    __asm__ volatile("syscall"
                     : "=r"(out)
                     : "r"(_s), "r"(_a), "r"(_b), "r"(_c), "r"(_d));
    return out;
}
#elif defined(__i386__)
static int manual_syscall(long s, long a, long b, long c, long d) {
    REG(s, eax);
    OREG(out, eax);
    __asm__ volatile("push %5; push %4; push %3; push %2; push $0;"
                     "mov %%esp, %%ecx;"
                     "call 1f; 1: pop %%edx; add $(2f-1b), %%edx;"
                     "sysenter;"
                     "2: add $(5 * 4), %%esp"
                     : "=r"(out)
                     : "r"(_s), "g"(a), "g"(b), "g"(c), "g"(d)
                     : "edx", "ecx");
    return out;
}
#elif defined(__arm__)
static int manual_syscall(long s, long a, long b, long c, long d) {
    REG(s, r12); REG(a, r0); REG(b, r1); REG(c, r2); REG(d, r3);
    OREG(out, r0);
    __asm__ volatile("svc #0x80"
                     : "=r"(out)
                     : "r"(_s), "r"(_a), "r"(_b), "r"(_c), "r"(_d));
    return out;
}
#elif defined(__arm64__)
static int manual_syscall(long s, long a, long b, long c, long d) {
    REG(s, x16); REG(a, x0); REG(b, x1); REG(c, x2); REG(d, x3);
    OREG(out, x0);
    __asm__ volatile("svc #0x80"
                     : "=r"(out)
                     : "r"(_s), "r"(_a), "r"(_b), "r"(_c), "r"(_d));
    return out;
}
#else
#error ?
#endif
