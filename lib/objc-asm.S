/* These all forward to the IMP obtained from block->get_imp(block); this
 * signature is just so that I don't have to have two different assembly
 * implementations for the temporary and fake-old blocks.  To make things
 * confusing, we don't know whether it will be called as stret or not, so we
 * have to guess.
 * (Could avoid the guessing by generating custom assembly instead, like
 * Substrate does, but meh.)
 */
.globl _temp_block_invoke
.section __TEXT,__text,regular,pure_instructions
#if defined(__x86_64__)
_temp_block_invoke:
    push %rcx
    mov 0x10(%rsi), %rax /* block if stret, else self */
    lea _temp_block_invoke(%rip), %rcx
    cmp %rax, %rcx
    pop %rcx
    je 2f

    mov 0x20(%rdi), %rax /* get_imp */
    push %rdi; push %rsi; push %rdx; push %rcx; push %r8; push %r9
    call *%rax
    pop %r9; pop %r8; pop %rcx; pop %rdx; pop %rdi; pop %rsi
    mov 0x28(%rsi), %rsi /* selector */
    jmp *%rax
2: /* stret */
    mov 0x20(%rsi), %rax /* get_imp */
    push %rdi; push %rsi; push %rdx; push %rcx; push %r8; push %r9
    mov %rsi, %rdi
    call *%rax
    pop %r9; pop %r8; pop %rcx; pop %rsi; pop %rdx; pop %rdi
    mov 0x28(%rdx), %rdx /* selector */
    jmp *%rax
#elif defined(__i386__)
_temp_block_invoke:
    call 1f
1:
    pop %edx
    lea _temp_block_invoke-1b(%edx), %edx
    mov 8(%esp), %ecx /* block if stret, else self */
    mov 0xc(%ecx), %eax
    cmp %eax, %edx
    je 2f

    mov 4(%esp), %eax /* block */
    push %eax
    mov 0x14(%eax), %eax /* get_imp */
    call *%eax
    add $4, %esp
    mov 8(%esp), %edx /* self */
    mov 4(%esp), %ecx /* block */
    mov %edx, 4(%esp)
    mov 0x18(%ecx), %ecx
    mov %ecx, 8(%esp)
    jmp *%eax
2: /* stret */
    int3
    mov 8(%esp), %eax /* block */
    push %eax
    mov 0x14(%eax), %eax /* get_imp */
    call *%eax
    add $4, %esp
    int3
    mov 12(%esp), %ecx /* self */
    mov %ecx, 8(%esp)
    mov 8(%esp), %ecx /* block */
    mov 0x18(%ecx), %ecx
    mov %ecx, 12(%esp)
    jmp *%eax
#elif defined(__arm__)
.thumb_func _temp_block_invoke
.thumb
_temp_block_invoke:
1:
    ldr r9, [r1, #0xc]
    adr r12, 1b
    cmp r9, r12
    beq 2f

    push {r0-r4, lr} /* r4 for align */
    ldr r9, [r0, #0x14]
    blx r9
    mov r12, r0
    pop {r0-r4, lr}
    mov r9, r0
    mov r0, r1
    ldr r1, [r9, #0x18]
    bx r12
2: /* stret */
    push {r0-r3, lr}
    ldr r9, [r1, #0x14]
    blx r9
    mov r12, r0
    pop {r0-r3, lr}
    mov r9, r1
    mov r1, r2
    ldr r2, [r9, #0x18]
    bx r12
#elif defined(__arm64__)
.align 2
_temp_block_invoke:
    stp x7, x6, [sp, #-0x10]!
    stp x5, x4, [sp, #-0x10]!
    stp x3, x2, [sp, #-0x10]!
    stp x1, x0, [sp, #-0x10]!
    str x30, [sp, #-0x10]!

    ldr x9, [x0, #0x20]
    blr x9
    mov x10, x0

    ldr x30, [sp], #0x10
    ldp x0, x9, [sp], #0x10
    ldp x3, x2, [sp], #0x10
    ldp x5, x4, [sp], #0x10
    ldp x7, x6, [sp], #0x10

    ldr x1, [x9, #0x28]
    br x10
#else
#error No forwarding assembly definition for this arch
#endif
