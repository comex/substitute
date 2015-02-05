#pragma once
#include <stdlib.h>
/*
prefixes REX opc ModR/M SIB displacement immediate

1A/C: modrm stuff
i64: 32 only
o64: 64 only

CDEGMNPQRSUVW: modrm
EMQW: modrm w/ address
IJO: immediate
L: 8-bit immediate

a Two one-word operands in memory or two double-word operands in memory, depending on operand-size attribute (used only by the BOUND instruction).
b Byte, regardless of operand-size attribute.
c Byte or word, depending on operand-size attribute.
d Doubleword, regardless of operand-size attribute.
dq Double-quadword, regardless of operand-size attribute.
p 32-bit, 48-bit, or 80-bit pointer, depending on operand-size attribute.
pd 128-bit or 256-bit packed double-precision floating-point data. pi Quadword MMX technology register (for example: mm0).
ps 128-bit or 256-bit packed single-precision floating-point data. q Quadword, regardless of operand-size attribute.
qq Quad-Quadword (256-bits), regardless of operand-size attribute. s 6-byte or 10-byte pseudo-descriptor.
sd Scalar element of a 128-bit double-precision floating data.
ss Scalar element of a 128-bit single-precision floating data.
si Doubleword integer register (for example: eax).
v Word, doubleword or quadword (in 64-bit mode), depending on operand-size attribute.
w Word, regardless of operand-size attribute.
x dq or qq based on the operand-size attribute.
y Doubleword or quadword (in 64-bit mode), depending on operand-size attribute.
z Word for 16-bit operand-size or doubleword for 32 or 64-bit operand-size.

*/
#define REP4(x) x, x, x, x
#define REP8(x) REP4(x), REP4(x)
#define REP16(x) REP8(x), REP8(x)
#define I_8   0x01
#define I_16  0x02
#define I_24  0x04
#define I_v   0x04
#define I_z   0x05
#define I_p   0x06
#define I_MOD 0x08
#define I_ADD 0x10
#define I_MODA (I_MOD|I_ADD)
#define I_PFX 0x20
#define I_BAD 0x80
#define I_SPECIAL 0 /* tested manually - just sticking it there for documentation */
#ifdef TARGET_x86_64
#define if64(_64, _32) _64
#else
#define if64(_64, _32) _32
#endif
#define i64(x) if64(I_BAD, x)
#define o64(x) if64(x, I_BAD)

static const uint8_t onebyte_bits[] = {
/* todo add right side */
/*0x*/ REP4(I_MODA), I_8, I_v, i64(0), i64(0), REP4(I_MODA), I_8, I_z, i64(0), I_SPECIAL,
/*1x*/ REP4(I_MODA), I_8, I_v, i64(0), i64(0), REP4(I_MODA), I_8, I_z, i64(0), i64(0),
/*2x*/ REP4(I_MODA), I_8, I_v, I_PFX,  i64(0), REP4(I_MODA), I_8, I_z, I_PFX,  i64(0),
/*3x*/ REP4(I_MODA), I_8, I_v, I_PFX,  i64(0), REP4(I_MODA), I_8, I_z, I_PFX,  i64(0),
/*4x*/ REP16(if64(PFX, 0)),
/*5x*/ REP16(0),
/*6x*/ i64(0), i64(0), i64(I_MOD), I_MOD|I_ADD, I_PFX, I_PFX, I_PFX, I_PFX,
            I_z, I_MODA|I_z, I_8, I_MODA|I_8, REP4(0),
/*7x*/ REP16(I_8),
/*8x*/ I_MODA|I_8, I_MODA|I_v, i64(I_MODA|I_8), I_MODA|I_8, I_MODA|I_8, I_MODA|I_v, I_MODA|I_8, I_MODA|I_v,
            REP4(I_MODA), I_MOD, I_MODA, I_MOD, I_8|I_SPECIAL,
/*9x*/ REP8(0), 0, 0, i64(0), 0, 0, 0, 0, 0,
/*Ax*/ I_8, I_v, I_8, I_v, REP4(0), I_8, I_z,0, 0, 0, 0, 0, 0,
/*Bx*/ REP8(I_8), REP8(I_v),
/*Cx*/ I_MODA|I_8, I_MODA|I_8, I_16, 0, i64(I_MODA), i64(I_MODA), I_MODA|I_8, I_MODA|I_8,
            I_24, 0, I_16, 0, 0, I_8, i64(0), 0,
/*Dx*/ REP4(I_MODA), i64(I_8), i64(I_8), I_BAD, 0, REP8(I_SPECIAL),
/*Ex*/ REP8(I_8), I_z, I_z, I_p, I_8, 0, 0, 0, 0,
/*Fx*/ I_PFX, I_BAD, I_PFX, I_PFX, 0, 0, I_MODA, I_MODA, 0, 0, 0, 0, 0, 0, I_8|I_SPECIAL, I_8|I_SPECIAL,
};
_Static_assert(sizeof(onebyte_bits) == 256, "onebyte_bits");
