/* These all try to re-invoke old_ptr.  To make things worse, we don't know
 * whether it will be called as stret or not, so we have to guess. */
.globl _temp_block_invoke
.section __TEXT,__text,regular,pure_instructions
#if defined(__x86_64__)
_temp_block_invoke:
    push %rcx
    mov 0x10(%rsi), %rax; /* block if stret, else self */
    lea _temp_block_invoke(%rip), %rcx
    cmp %rax, %rcx
    pop %rcx
    je 2f
    mov %rdi, %rax
    mov %rsi, %rdi
    mov 0x28(%rax), %rsi
    mov 0x20(%rax), %rax
    jmp *(%rax)
2: /* stret */
    mov %rsi, %rax
    mov %rdx, %rsi
    mov 0x28(%rsi), %rdx
    mov 0x20(%rsi), %rax
    jmp *(%rax)
#elif defined(__i386__)
_temp_block_invoke:
    call 1f
1:
    pop %edx
    lea _temp_block_invoke-1b(%edx), %edx
    mov 8(%esp), %ecx; /* block if stret, else self */
    mov 0xc(%ecx), %eax
    cmp %eax, %edx
    je 2f
    mov 4(%esp), %eax; /* block */
    mov %ecx, 4(%esp)
    mov 0x18(%eax), %ecx
    mov %ecx, 8(%esp)
    mov 0x14(%eax), %eax
    jmp *(%eax)
2: /* stret */
    mov 12(%esp), %ecx; /* self */
    mov 8(%esp), %eax; /* block */
    mov %ecx, 4(%esp)
    mov 0x18(%eax), %ecx
    mov %ecx, 12(%esp)
    mov 0x14(%eax), %eax
    jmp *(%eax)
#elif defined(__arm__)
.thumb_func _temp_block_invoke
.thumb
_temp_block_invoke:
1:
    ldr r9, [r1, #0xc]
    adr r12, 1b
    cmp r9, r12
    beq 2f
    mov r9, r0
    mov r0, r1
    ldr r1, [r9, #0x18]
    ldr r9, [r9, #0x14]
    ldr r9, [r9]
    bx r9
2: /* stret */
    mov r9, r1
    mov r1, r2
    ldr r2, [r9, #0x18]
    ldr r9, [r9, #0x14]
    ldr r9, [r9]
    bx r9
#elif defined(__arm64__)
.align 2
_temp_block_invoke:
    mov x9, x0
    mov x0, x1
    ldr x1, [x9, #0x28]
    ldr x9, [x9, #0x20]
    ldr x9, [x9]
    br x9
#else
#error No forwarding assembly definition for this arch
#endif
