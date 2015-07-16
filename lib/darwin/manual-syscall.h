#pragma once

#define GEN_SYSCALL(name, num) \
    __asm__(".private_extern _manual_" #name "\n" \
            ".pushsection __TEXT,__text,regular,pure_instructions\n" \
            GEN_SYSCALL_PRE(name) \
            "_manual_" #name ":\n" \
            ".set num, " #num "\n" \
            GEN_SYSCALL_INNER() \
            ".popsection\n")
#define GEN_SYSCALL_PRE(name)

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
#ifdef __thumb__
#undef GEN_SYSCALL_PRE
#define GEN_SYSCALL_PRE(name) \
    ".thumb_func _manual_" #name "\n" \
    ".align 2\n"
#endif
#define GEN_SYSCALL_INNER() \
    "mov r12, sp\n" \
    "push {r4-r6}\n" \
    "ldm r12, {r4-r6}\n" \
    "mov r12, #num\n" \
    "svc #0x80\n" \
    "pop {r4-r6}\n" \
    "bx lr\n"
#elif defined(__arm64__)
#define GEN_SYSCALL_INNER() \
    "mov x16, #num\n" \
    "svc #0x80\n" \
    "ret\n"
#else
#error ?
#endif
