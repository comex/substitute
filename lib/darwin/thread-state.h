#pragma once
#include <stdint.h>

struct _x86_thread_state_32 {
    uint32_t eax, ebx, ecx, edx, edi, esi, ebp, esp;
    uint32_t ss, eflags, eip, cs, ds, es, fs, gs;
};
#define _x86_thread_state_32_flavor 1
struct _x86_thread_state_64 {
    uint64_t rax, rbx, rcx, rdx, rdi, rsi, rbp, rsp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip, rflags, cs, fs, gs;
};
#define _x86_thread_state_64_flavor 4
struct _arm_thread_state_32 {
    uint32_t r[13], sp, lr, pc, cpsr;
};
#define _arm_thread_state_32_flavor 9
struct _arm_thread_state_64 {
    uint64_t x[29], fp, lr, sp, pc;
    uint32_t cpsr, pad;
};
#define _arm_thread_state_64_flavor 6

