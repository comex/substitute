#ifdef THUMB2
.thumb
.thumb_func
.syntax unified
#endif
.global foo
foo:
strex r0, r1, [pc]
mov r0, pc
#ifndef THUMB2
add r0, pc, r1
add r0, r1, pc
str pc, [r0], #5
str pc, [r0], r3
#endif
add r0, pc, #123
mcr p15,0,pc,c14,c3,0
str r3, [pc, #5]
str pc, [pc, #5]
#ifdef THUMB2 /* it shouldn't be, though */
strht r0, [r3]
#endif
ldr r0, [pc]
ldr r0, [pc, #5]
ldr pc, [r0], #5
ldr pc, [pc]
#ifndef THUMB2
ldr pc, [r2], r7
adr pc, 1f
#endif
adr r3, 1f

ldrb  r1, [pc]
ldrsb r1, [pc]
ldrh  r1, [pc]
ldrsh r1, [pc]
ldr   r1, [pc]
ldrd  r0, r1, [pc]
str   r1, [pc]
strd  r0, r1, [pc]

/* push {r0-r3, pc} */
push {r0-r3, lr}
/* pop {r0-r3, pc} */

1:
.long 0xdeadbeef
.long 0xdeadbeef
