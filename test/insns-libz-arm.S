#ifdef THUMB2
.thumb
.thumb_func
.syntax unified
#endif
_deflateInit2_:
@ BB#0:
	push	{r4, r5, r6, r7, lr}
	add	r7, sp, #12
	sub	sp, #88
	ldr.w	r9, [r7, #20]
	ldr.w	r12, [r7, #16]
	ldr.w	lr, [r7, #12]
	ldr	r4, [r7, #8]
	movs	r5, #0
	movt	r5, #0
	movs	r6, #1
	movt	r6, #0
#if 0 /* just cutting out some insns so it all fits */
	str	r0, [sp, #80]
	str	r1, [sp, #76]
	str	r2, [sp, #72]
	str	r3, [sp, #68]
	str	r4, [sp, #64]
	str.w	lr, [sp, #60]
	str.w	r12, [sp, #56]
	str.w	r9, [sp, #52]
	str	r6, [sp, #44]
	ldr	r0, [sp, #56]
#endif
	cmp	r0, r5
	beq	LBB1_3
@ BB#1:
	movw	r0, #123
	movt	r0, #456
LPC1_0:
	add	r0, pc
	ldr	r1, [sp, #56]
	ldrsb.w	r1, [r1]
	ldrsb.w	r0, [r0]
	cmp	r1, r0
	bne	LBB1_3
@ BB#2:
	ldr	r0, [sp, #52]
	cmp	r0, #56
	beq	LBB1_4
LBB1_3:
	movw	r0, #65530
	movt	r0, #65535
	str	r0, [sp, #84]
	b	LBB1_37
LBB1_4:
	movs	r0, #0
	movt	r0, #0
	ldr	r1, [sp, #80]
	cmp	r1, r0
	bne	LBB1_6
@ BB#5:
	movw	r0, #65534
	movt	r0, #65535
	str	r0, [sp, #84]
	b	LBB1_37
LBB1_6:
	movs	r0, #0
	movt	r0, #0
	ldr	r1, [sp, #80]
	str	r0, [r1, #24]
	ldr	r1, [sp, #80]
	ldr	r1, [r1, #32]
	cmp	r1, r0
	bne	LBB1_8
@ BB#7:
	movs	r0, #0
	movt	r0, #0
	movw	r1, #123
	movt	r1, #456
LPC1_1:
	add	r1, pc
	ldr	r1, [r1]
	ldr	r2, [sp, #80]
	str	r1, [r2, #32]
	ldr	r1, [sp, #80]
	str	r0, [r1, #40]
LBB1_8:
	movs	r0, #0
	movt	r0, #0
	ldr	r1, [sp, #80]
	ldr	r1, [r1, #36]
	cmp	r1, r0
	bne	LBB1_10
@ BB#9:
	movw	r0, #123
	movt	r0, #456
LPC1_2:
	add	r0, pc
	ldr	r0, [r0]
	ldr	r1, [sp, #80]
	str	r0, [r1, #36]
LBB1_10:
	ldr	r0, [sp, #76]
	cmn.w	r0, #1
	bne	LBB1_12
@ BB#11:
	movs	r0, #6
	movt	r0, #0
	str	r0, [sp, #76]
LBB1_12:
	ldr	r0, [sp, #68]
	cmp	r0, #0
	bge	LBB1_14
@ BB#13:
	movs	r0, #0
	movt	r0, #0
	str	r0, [sp, #44]
	ldr	r1, [sp, #68]
	subs	r0, r0, r1
	str	r0, [sp, #68]
	b	LBB1_17
LBB1_14:
	ldr	r0, [sp, #68]
	cmp	r0, #15
	ble	LBB1_16
@ BB#15:
	movs	r0, #2
	movt	r0, #0
	str	r0, [sp, #44]
	ldr	r0, [sp, #68]
	subs	r0, #16
	str	r0, [sp, #68]
LBB1_16:
	b	LBB1_17
LBB1_17:
	ldr	r0, [sp, #64]
	cmp	r0, #1
	blt	LBB1_26
@ BB#18:
	ldr	r0, [sp, #64]
	cmp	r0, #9
	bgt	LBB1_26
@ BB#19:
	ldr	r0, [sp, #72]
	cmp	r0, #8
	bne	LBB1_26
@ BB#20:
	ldr	r0, [sp, #68]
	cmp	r0, #8
	blt	LBB1_26
@ BB#21:
	ldr	r0, [sp, #68]
	cmp	r0, #15
	bgt	LBB1_26
@ BB#22:
	ldr	r0, [sp, #76]
	cmp	r0, #0
	blt	LBB1_26
@ BB#23:
	ldr	r0, [sp, #76]
	cmp	r0, #9
	bgt	LBB1_26
@ BB#24:
	ldr	r0, [sp, #60]
	cmp	r0, #0
	blt	LBB1_26
@ BB#25:
	ldr	r0, [sp, #60]
	cmp	r0, #4
	ble	LBB1_27
LBB1_26:
	movw	r0, #65534
	movt	r0, #65535
	str	r0, [sp, #84]
	b	LBB1_37
LBB1_27:
	ldr	r0, [sp, #68]
	cmp	r0, #8
	bne	LBB1_29
@ BB#28:
	movs	r0, #9
	movt	r0, #0
	str	r0, [sp, #68]
LBB1_29:
	movs	r0, #0
	movt	r0, #0
	movs	r1, #1
	movt	r1, #0
	movw	r2, #5828
	movt	r2, #0
	ldr	r3, [sp, #80]
	ldr	r3, [r3, #32]
	ldr.w	r9, [sp, #80]
	ldr.w	r9, [r9, #40]
	str	r0, [sp, #36]           @ 4-byte Spill
	mov	r0, r9
	blx	r3
	str	r0, [sp, #48]
	ldr	r0, [sp, #48]
	ldr	r1, [sp, #36]           @ 4-byte Reload
	cmp	r0, r1
	bne	LBB1_31
@ BB#30:
	movw	r0, #65532
	movt	r0, #65535
	str	r0, [sp, #84]
	b	LBB1_37
LBB1_31:
	movs	r0, #0
	movt	r0, #0
	movs	r2, #4
	movt	r2, #0
	movs	r1, #1
	movt	r1, #0
	movs	r3, #2
	movt	r3, #0
	movw	r9, #3
	movt	r9, #0
	ldr.w	r12, [sp, #48]
	ldr.w	lr, [sp, #80]
	str.w	r12, [lr, #28]
	ldr.w	r12, [sp, #80]
	ldr.w	lr, [sp, #48]
	str.w	r12, [lr]
	ldr.w	r12, [sp, #44]
	ldr.w	lr, [sp, #48]
	str.w	r12, [lr, #24]
	ldr.w	r12, [sp, #48]
	str.w	r0, [r12, #28]
	ldr.w	r12, [sp, #68]
	ldr.w	lr, [sp, #48]
	str.w	r12, [lr, #48]
	ldr.w	r12, [sp, #48]
	ldr.w	r12, [r12, #48]
	lsl.w	r12, r1, r12
	ldr.w	lr, [sp, #48]
	str.w	r12, [lr, #44]
	ldr.w	r12, [sp, #48]
	ldr.w	r12, [r12, #44]
	sub.w	r12, r12, #1
	ldr.w	lr, [sp, #48]
	str.w	r12, [lr, #52]
	ldr.w	r12, [sp, #64]
	add.w	r12, r12, #7
	ldr.w	lr, [sp, #48]
	str.w	r12, [lr, #80]
	ldr.w	r12, [sp, #48]
	ldr.w	r12, [r12, #80]
	lsl.w	r12, r1, r12
	ldr.w	lr, [sp, #48]
	str.w	r12, [lr, #76]
	ldr.w	r12, [sp, #48]
	ldr.w	r12, [r12, #76]
	sub.w	r12, r12, #1
	ldr.w	lr, [sp, #48]
	str.w	r12, [lr, #84]
	ldr.w	r12, [sp, #48]
	ldr.w	r12, [r12, #80]
	add.w	r12, r12, #3
	sub.w	r12, r12, #1
	str	r0, [sp, #32]           @ 4-byte Spill
	mov	r0, r12
	str	r1, [sp, #28]           @ 4-byte Spill
	mov	r1, r9
	str	r3, [sp, #24]           @ 4-byte Spill
	str	r2, [sp, #20]           @ 4-byte Spill
	bl	___udivsi3
	ldr	r1, [sp, #48]
	str	r0, [r1, #88]
	ldr	r0, [sp, #80]
	ldr	r0, [r0, #32]
	ldr	r1, [sp, #80]
	ldr	r1, [r1, #40]
	ldr	r2, [sp, #48]
	ldr	r2, [r2, #44]
	str	r0, [sp, #16]           @ 4-byte Spill
	mov	r0, r1
	mov	r1, r2
	ldr	r2, [sp, #24]           @ 4-byte Reload
	ldr	r3, [sp, #16]           @ 4-byte Reload
	blx	r3
	ldr	r1, [sp, #48]
	str	r0, [r1, #56]
	ldr	r0, [sp, #80]
	ldr	r0, [r0, #32]
	ldr	r1, [sp, #80]
	ldr	r1, [r1, #40]
	ldr	r2, [sp, #48]
	ldr	r2, [r2, #44]
	str	r0, [sp, #12]           @ 4-byte Spill
	mov	r0, r1
	mov	r1, r2
	ldr	r2, [sp, #24]           @ 4-byte Reload
	ldr	r3, [sp, #12]           @ 4-byte Reload
	blx	r3
	ldr	r1, [sp, #48]
	str	r0, [r1, #64]
	ldr	r0, [sp, #80]
	ldr	r0, [r0, #32]
	ldr	r1, [sp, #80]
	ldr	r1, [r1, #40]
	ldr	r2, [sp, #48]
	ldr	r2, [r2, #76]
	str	r0, [sp, #8]            @ 4-byte Spill
	mov	r0, r1
	mov	r1, r2
	ldr	r2, [sp, #24]           @ 4-byte Reload
	ldr	r3, [sp, #8]            @ 4-byte Reload
	blx	r3
	ldr	r1, [sp, #48]
	str	r0, [r1, #68]
	ldr	r0, [sp, #48]
	add.w	r0, r0, #5824
	ldr	r1, [sp, #32]           @ 4-byte Reload
	str	r1, [r0]
	ldr	r0, [sp, #64]
	adds	r0, #6
	ldr	r2, [sp, #28]           @ 4-byte Reload
	lsl.w	r0, r2, r0
	ldr	r3, [sp, #48]
	movw	r9, #5788
	movt	r9, #0
	add	r3, r9
	str	r0, [r3]
	ldr	r0, [sp, #80]
	ldr	r0, [r0, #32]
	ldr	r3, [sp, #80]
	ldr	r3, [r3, #40]
	ldr.w	r9, [sp, #48]
	movw	r12, #5788
	movt	r12, #0
	add	r9, r12
	ldr.w	r1, [r9]
	str	r0, [sp, #4]            @ 4-byte Spill
	mov	r0, r3
	ldr	r2, [sp, #20]           @ 4-byte Reload
	ldr	r3, [sp, #4]            @ 4-byte Reload
	blx	r3
	str	r0, [sp, #40]
	ldr	r0, [sp, #40]
	ldr	r1, [sp, #48]
	str	r0, [r1, #8]
	ldr	r0, [sp, #48]
	movw	r1, #5788
	movt	r1, #0
	add	r0, r1
	ldr	r0, [r0]
	lsls	r0, r0, #2
	ldr	r1, [sp, #48]
	str	r0, [r1, #12]
	ldr	r0, [sp, #48]
	ldr	r0, [r0, #56]
	ldr	r1, [sp, #32]           @ 4-byte Reload
	cmp	r0, r1
	beq	LBB1_35
@ BB#32:
	movs	r0, #0
	movt	r0, #0
	ldr	r1, [sp, #48]
	ldr	r1, [r1, #64]
	cmp	r1, r0
	beq	LBB1_35
@ BB#33:
	movs	r0, #0
	movt	r0, #0
	ldr	r1, [sp, #48]
	ldr	r1, [r1, #68]
	cmp	r1, r0
	beq	LBB1_35
@ BB#34:
	movs	r0, #0
	movt	r0, #0
	ldr	r1, [sp, #48]
	ldr	r1, [r1, #8]
	cmp	r1, r0
	bne	LBB1_36
LBB1_35:
	movw	r0, #123
	movt	r0, #456
LPC1_3:
	add	r0, pc
	ldr	r0, [r0]
	movw	r1, #666
	movt	r1, #0
	ldr	r2, [sp, #48]
	str	r1, [r2, #4]
	ldr	r0, [r0, #24]
	ldr	r1, [sp, #80]
	str	r0, [r1, #24]
	ldr	r0, [sp, #80]
	bl	_deflateEnd
	movw	r1, #65532
	movt	r1, #65535
	str	r1, [sp, #84]
	str	r0, [sp]                @ 4-byte Spill
	b	LBB1_37
LBB1_36:
	ldr	r0, [sp, #40]
	ldr	r1, [sp, #48]
	movw	r2, #5788
	movt	r2, #0
	add	r1, r2
	ldr	r1, [r1]
	movs	r2, #1
	movt	r2, #0
	lsrs	r1, r2
	lsls	r1, r1, #1
	add	r0, r1
	ldr	r1, [sp, #48]
	movw	r2, #5796
	movt	r2, #0
	add	r1, r2
	str	r0, [r1]
	ldr	r0, [sp, #48]
	ldr	r0, [r0, #8]
	ldr	r1, [sp, #48]
	movw	r2, #5788
	movt	r2, #0
	add	r1, r2
	ldr	r1, [r1]
	movs	r2, #3
	movt	r2, #0
	muls	r1, r2, r1
	add	r0, r1
	ldr	r1, [sp, #48]
	movw	r2, #5784
	movt	r2, #0
	add	r1, r2
	str	r0, [r1]
	ldr	r0, [sp, #76]
	ldr	r1, [sp, #48]
	str.w	r0, [r1, #132]
	ldr	r0, [sp, #60]
	ldr	r1, [sp, #48]
	str.w	r0, [r1, #136]
	ldr	r0, [sp, #72]
	ldr	r1, [sp, #48]
	strb.w	r0, [r1, #36]
	ldr	r0, [sp, #80]
	bl	_deflateReset
	str	r0, [sp, #84]
LBB1_37:
	ldr	r0, [sp, #84]
	add	sp, #88
	pop	{r4, r5, r6, r7, pc}


