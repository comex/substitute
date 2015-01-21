.text
.align 2
/* sp -> {pthread_create, dlopen, dylib} */
#if defined(__x86_64__)
    lea -8(%rsp), %rdi /* thread */
    xor %rsi, %rsi /* attr */
	lea thread_func(%rip), %rdx /* start_routine */
	mov %rsp, %rcx /* arg */
    mov %rdi, %rsp
    call *(%rcx)
/* suicide */
    mov $361, %rax /* bsdthread_terminate */
    xor %rdi, %rdi /* stackaddr */
    xor %rsi, %rsi /* freesize */
    xor %rdx, %rdx /* port */
    xor %rcx, %rcx /* sem */
    syscall
/* still here? */
    mov $0xbad, %rax
    jmp *%rax

thread_func:
    mov 0x8(%rdi), %rax /* dlopen */
    mov 0x10(%rdi), %rdi /* dylib */
    xor %rsi, %rsi
    jmp *%rax

#elif defined(__i386__)

    mov %esp, %ecx
    push %ecx /* arg */
    call 1f
1:
    pop %eax
    add $(thread_func - 1b), %eax
    push %eax /* start_routine */
    xor %eax, %eax
    push %eax /* attr */
    push %esp /* thread */
    call *(%ecx)
/* suicide */
    mov $361, %eax /* bsdthread_terminate */
    xor %edx, %edx
    push %edx /* sem */
    push %edx /* port */
    push %edx /* freesize */
    push %edx /* stackaddr */
    syscall
/* still here? */
    mov $0xbad, %eax
    jmp *%eax

thread_func:
    xor %edx, %edx
    push %edx
    mov 0x4(%esp), %ecx /* arg */
    mov 0x8(%ecx), %edx /* dylib */
    push %edx
    mov 0x4(%ecx), %edx /* dlopen */
    push %edx
    call *%eax
    add $8, %esp
    ret

#elif defined(__arm__)

    sub sp, #4
    mov r0, sp
    mov r1, #0
    adr r2, thread_func
    add r3, sp, #4
    ldr r9, [r3]
    blx r9
/* suicide */
    mov r0, #0
    mov r1, #0
    mov r2, #0
    mov r3, #0
    mov r12, #361
    svc #0x80
/* still here? */
    mov r0, #0xbad
    bx r0
thread_func:
    ldr r2, [r0]
    ldr r0, [r0, #4]
    mov r1, #0
    bx r2

#elif defined(__arm64__)
    sub sp, sp, #8
    mov x0, sp
    mov x1, #0
    adr x2, 1f
    add x3, sp, #4
    ldr x9, [x3]
    blr x9
/* suicide */
    mov x0, #0
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x12, #361 /* ??? */
    svc #0x80
/* still here? */
    mov x0, #0xbad
    br x0
1:
    ldr x2, [x0]
    ldr x0, [x0, #8]
    mov x1, #0
    br x2
#endif
