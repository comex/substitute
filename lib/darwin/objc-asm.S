#include "objc.h"
.text
.align _PAGE_SHIFT
#ifdef __arm__
.thumb_func _remap_start
.thumb
#endif
.private_extern _remap_start
_remap_start:

.set i, 0
#define my_rpe (0b +   (_PAGE_SIZE \
                      - i * TRAMPOLINE_SIZE \
                      + i * TRAMP_INFO_PAGE_ENTRY_SIZE))
.rept TRAMPOLINES_PER_PAGE
0:
#if defined(__x86_64__)
    /* double push for align */
    push %rdi; push %rsi; push %rdx; push %rcx; push %r8; push %r9; push %r9
    lea my_rpe(%rip), %rdx
    mov 8(%rdx), %rdi
    mov 16(%rdx), %rsi
    call *(%rdx)
    pop %r9; pop %r9; pop %r8; pop %rcx; pop %rdx; pop %rsi; pop %rdi
    jmp *%rax
#elif defined(__i386__)
    call 1f
1:
    pop %edx
    lea my_rpe-1b(%edx), %edx
    push 8(%edx)
    push 4(%edx)
    call *(%edx)
    add $$8, %esp
    jmp *%eax
#elif defined(__arm__)
    push {r0-r4, lr} /* r4 for align */
    mov r3, #:lower16:(my_rpe - (1f + 2))
    add r3, pc
1:
    ldr r0, [r3, #4]
    ldr r1, [r3, #8]
    ldr r2, [r3]
    blx r2
    mov r9, r0
    pop {r0-r4, lr}
    bx r9
#elif defined(__arm64__)
    stp x30, x8, [sp, #-0x10]!
    stp x7, x6, [sp, #-0x10]!
    stp x5, x4, [sp, #-0x10]!
    stp x3, x2, [sp, #-0x10]!
    stp x1, x0, [sp, #-0x10]!

    ldr x0, my_rpe+8
    ldr x1, my_rpe+0x10
    ldr x2, my_rpe
    blr x2
    mov x9, x0

    ldp x1, x0, [sp], #0x10
    ldp x3, x2, [sp], #0x10
    ldp x5, x4, [sp], #0x10
    ldp x7, x6, [sp], #0x10
    ldp x30, x8, [sp], #0x10

    br x9
#else
#error No forwarding assembly definition for this arch
#endif

.set i, i + 1
.endr

