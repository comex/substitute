#pragma once

#define GEN_SYSCALL(name, num) \
    __asm__(".globl _manual_" #name "\n" \
            ".pushsection __TEXT,__text,regular,pure_instructions\n" \
            "_manual_" #name ":\n" \
            ".set num, " #num "\n" \
            GEN_SYSCALL_INNER() \
            ".popsection\n")

#if defined(__x86_64__)
/* Look at me, I'm different! */
#define GEN_SYSCALL_INNER() \
    ".if num < 0\n" \
        "mov $(-num | 1 << 24), %eax\n" \
    ".else\n" \
        "mov $( num | 2 << 24), %eax\n" \
    ".endif\n" \
    "mov %rcx, %r10\n" \
    "syscall\n" \
    "ret\n"

#elif defined(__i386__)
#define GEN_SYSCALL_INNER() \
    "mov $num, %eax\n" \
    "call 0f\n" \
    "0: pop %edx\n" \
    "add $(1f-0b), %edx\n" \
    "mov %esp, %ecx\n" \
    "sysenter\n" \
    "1: ret\n"
#elif defined(__arm__)
#define GEN_SYSCALL_INNER() \
    "mov r12, #num\n" \
    "svc #0x80\n" \
    "bx lr\n"
#elif defined(__arm64__)
#define GEN_SYSCALL_INNER() \
    "mov x12, #num\n" \
    "svc #0x80\n" \
    "ret\n"
#else
#error ?
#endif
