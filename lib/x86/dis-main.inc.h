/*
random notes:

REX: 0100wrxb

prefixes REX opc ModR/M SIB displacement immediate

1A/C: modrm stuff
i64: 32 only
o64: 64 only

CDEGMNPQRSUVW: modrm
EMQW: modrm w/ address
IJO: immediate
L: 8-bit immediate

VEX last byte 1:0: {none, 66, f3, f2}

*/


/* This is probably not the most efficient implementation, but hopefully good
 * enough... */

#define REP4(x) x, x, x, x
#define REP8(x) REP4(x), REP4(x)
#define REP16(x) REP8(x), REP8(x)
#define I_8    0x01
#define I_16   0x02
#define I_24   0x03
#define I_32   0x04
#define I_v    0x05
#define I_z    0x06
#define I_p    0x07
#define I_IMM_MASK 0x07
#define I_MOD  0x08
#define I_ADDR 0x10
#define I_MODA (I_MOD|I_ADDR)
/* mutually exclusive types */
#define I_PFX  0x20 /* prefix */
#define I_JMP  0x40 /* execution does not continue after this */
#define I_SPEC 0x60 /* special case */
#define I_TYPE_MASK 0x60
#define I_JIMM_ONLY 0x80 /* imm is jump offset */
#define I_JIMM (0x80|I_JMP)
#define I_BAD  0x80
#define I_CALL 0x100 /* not really in the table */
#ifdef TARGET_x86_64
#define if64(_64, _32) _64
#else
#define if64(_64, _32) _32
#endif
#define i64(x) if64(I_BAD, x)
#define o64(x) if64(x, I_BAD)

static const uint8_t onebyte_bits[] = {
/*00*/ REP4(I_MODA), I_8, I_z, i64(0), i64(0), REP4(I_MODA), I_8, I_z, i64(0), I_SPEC,
/*10*/ REP4(I_MODA), I_8, I_z, i64(0), i64(0), REP4(I_MODA), I_8, I_z, i64(0), i64(0),
/*20*/ REP4(I_MODA), I_8, I_z, I_PFX,  i64(0), REP4(I_MODA), I_8, I_z, I_PFX,  i64(0),
/*30*/ REP4(I_MODA), I_8, I_z, I_PFX,  i64(0), REP4(I_MODA), I_8, I_z, I_PFX,  i64(0),
/*40*/ REP16(if64(I_PFX, 0)),
/*50*/ REP16(0),
/*60*/ i64(0), i64(0), i64(I_MOD), I_MODA, I_PFX, I_PFX, I_PFX, I_PFX,
     /*68*/ I_z, I_MODA|I_z, I_8, I_MODA|I_8, REP4(0),
/*70*/ REP16(I_8|I_JIMM),
/*80*/ I_MODA|I_8, I_MODA|I_v, i64(I_MODA|I_8), I_MODA, I_MODA, I_MODA, I_MODA, I_MODA,
     /*88*/ REP4(I_MODA), I_MODA, I_MOD, I_MODA, if64(I_PFX, I_MODA),
/*90*/ REP8(0), 0, 0, i64(I_p), 0, 0, 0, 0, 0,
/*A0*/ I_8, I_v, I_8, I_v, REP4(0), I_8, I_z, 0, 0, 0, 0, 0, 0,
/*B0*/ REP8(I_8), REP8(I_v),
/*C0*/ I_MODA|I_8, I_MODA|I_8, I_16|I_JMP, I_JMP,
     /*C4*/ if64(I_PFX, I_MODA), if64(I_PFX, I_MODA), I_MODA|I_8, I_MODA|I_8,
     /*C8*/ I_24, 0, I_16|I_JMP, I_JMP, 0, I_8, i64(0), I_JMP,
/*D0*/ REP4(I_MODA), i64(I_8), i64(I_8), I_BAD, 0, REP8(I_SPEC),
            /* don't treat ljmp as a jump for now */
/*E0*/ REP4(I_8|I_JIMM), REP4(I_8),
     /*E8*/ I_z|I_JIMM_ONLY, I_z|I_JIMM, i64(I_p), I_8|I_JIMM, 0, 0, 0, 0,
/*F0*/ I_PFX, I_BAD, I_PFX, I_PFX, 0, 0, I_MODA, I_MODA,
     /*F8*/ 0, 0, 0, 0, 0, 0, I_MODA, I_SPEC,
};
_Static_assert(sizeof(onebyte_bits) == 256, "onebyte_bits");

/* Note:
    *All* currently defined 0f 38 opcodes are I_MODA.  Assuming that any
    unknown such opcodes are also I_MODA is probably better than generic
    unknown.
    Similarly, all defined 0f 3a opcodes are I_MODA|I_8.
*/

static const uint8_t _0f_bits[] = {
/*00*/ I_MODA, I_MODA, 0, 0, I_BAD, o64(0), 0, o64(0),
     /*08*/ 0, 0, I_BAD, 0, 0, I_MODA, 0, 0,
/*10*/ REP8(I_MODA), I_MODA, I_BAD, I_BAD, I_BAD, I_BAD, I_BAD, I_BAD, I_MODA,
/*20*/ REP4(I_MOD), REP4(I_BAD), REP8(I_MODA),
/*30*/ 0, 0, 0, 0, 0, 0, I_BAD, 0, I_MODA, I_BAD, I_MODA|I_8, I_BAD, REP4(I_BAD),
/*40*/ REP16(I_MODA),
/*50*/ I_MOD, I_MODA, I_MODA, I_MODA, REP4(I_MODA), REP8(I_MODA),
/*60*/ REP16(I_MODA),
/*70*/ I_MODA, I_MOD|I_8, I_MOD|I_8, I_MOD|I_8, I_MODA, I_MODA, I_MODA, 0,
     /*78*/ I_MODA, I_MODA, I_BAD, I_BAD, REP4(I_MODA),
/*80*/ REP16(I_z|I_JIMM),
/*90*/ REP16(I_MODA),
/*Ax*/ 0, 0, 0, 0, 0, 0, I_BAD, I_BAD,
     /*A8*/ 0, 0, 0, I_MODA, I_MODA|I_8, I_MODA, I_MODA, I_MODA,
/*B0*/ REP8(I_MODA), I_MODA, 0, I_MODA|I_8, I_MODA, REP4(I_MODA),
/*C0*/ I_MODA, I_MODA, I_MODA|I_8, I_MODA, I_MODA|I_8, I_MOD|I_8, I_MODA|I_8, I_MODA|I_z,
     /*C8*/ REP8(0),
/*D0*/ REP4(I_MODA), I_MODA, I_MODA, I_MODA, I_MOD, REP8(I_MODA),
/*E0*/ REP16(I_MODA),
/*F0*/ REP4(I_MODA), I_MODA, I_MODA, I_MODA, I_MOD,
     /*F8*/ REP4(I_MODA), I_MODA, I_MODA, I_MODA, I_BAD,
};
_Static_assert(sizeof(_0f_bits) == 256, "_0f_bits");

static void P(dis)(tdis_ctx ctx) {
    const uint8_t *orig = ctx->base.ptr;
    const uint8_t *ptr = ctx->base.ptr;

    int opnd_size = 4;
    int mod, rm = 0;
restart:;
    uint8_t byte1 = *ptr++;
    int bits = onebyte_bits[byte1];
    /* printf("b1=%x bytes=%x\n", byte1, bits); */
    if ((bits & I_TYPE_MASK) == I_SPEC) {
        if (byte1 == 0x0f) {
            uint8_t byte2 = *ptr++;
            bits = _0f_bits[byte2];
        } else if ((byte1 & 0xf8) == 0xd8) {
            /* ESC */
            ptr++;
            bits = I_MODA;
        } else if (byte1 == 0xff) {
            uint8_t modrm = *ptr;
            int subop = modrm >> 3 & 7;
            if (subop == 4 || subop == 5) /* JMP */
                bits = I_JMP | I_MODA;
            else if (subop == 2 || subop == 3) /* CALL */
                bits = I_CALL | I_MODA;
            else
                bits = I_MODA;
        } else {
            __builtin_abort();
        }
    }
got_bits: UNUSED
    if (bits == I_BAD)
        return P(bad)(ctx);
    if ((bits & I_TYPE_MASK) == I_PFX) {
        if (byte1 == 0x66) {
            opnd_size = 2;
            goto restart;
#ifdef TARGET_x86_64
        } else if ((byte1 & 0xf0) == 0x40) { /* REX */
            if (byte1 & 8) /* W */
                opnd_size = 8;
            if (byte1 & 1) /* B */
                rm = 8;
            goto restart;
        } else if (byte1 == 0xc4) { /* VEX 3 */
            uint8_t byte2 = *ptr++;
            if (!(byte2 & 0x20)) /* VEX.~B */
                rm = 8;
            UNUSED uint8_t byte3 = *ptr++;
            ptr++;
            int map = byte2 & 0x1f;
            switch (map) {
            case 1:
                bits = _0f_bits[byte2];
                break;
            case 2:
                bits = _0f_bits[0x38];
                break;
            case 3:
                bits = _0f_bits[0x3a];
                break;
            default:
                bits = I_BAD;
                break;
            }
            goto got_bits;
        } else if (byte1 == 0xc5) { /* VEX 2 */
            uint8_t byte2 = *ptr++;
            bits = _0f_bits[byte2];
            goto got_bits;
        } else if (byte1 == 0x8f) { /* XOP (AMD only) */
            uint8_t byte2 = *ptr;
            /* could be modrm */
            if ((byte2 >> 3 & 7) == 0)
                goto modrm;
            ptr++; /* ok, definitely XOP */
            if (!(byte2 & 0x20)) /* VEX.~B */
                rm = 8;
            int map = byte2 & 0x1f;
            switch (map) {
            case 8:
                bits = I_MODA|I_8;
                break;
            case 9:
                bits = I_MODA;
                break;
            case 10:
                bits = I_MODA|I_32;
                break;
            default:
                bits = I_BAD;
                break;
            }
            goto got_bits;
#endif
        } else {
            /* other prefix we don't care about */
            goto restart;
        }
    }
    UNUSED int modrm_off = ptr - orig;
    UNUSED uint8_t modrm;
    if (bits & I_MOD) {
    modrm: UNUSED;
        modrm = *ptr++;
        mod = modrm >> 6;
        rm |= modrm & 7;
        if (rm == 4) {
            /* sib */
            ptr++;
        }
        /* displacement */
#ifdef TARGET_x86_64
        if (mod == 0 && rm == 5)
            ptr += 4;
#endif
        else if (mod == 1)
            ptr++;
        else if (mod == 2)
            ptr += 4;
    }

    int imm_off = ptr - orig;

    /* disp */
    int imm_bits = bits & I_IMM_MASK;
    int imm_size;
    if (imm_bits <= I_32)
        imm_size = imm_bits;
    else if (imm_bits == I_v)
        imm_size = opnd_size;
    else if (imm_bits == I_z)
        imm_size = opnd_size == 2 ? 2 : 4;
    else if (imm_bits == I_p)
        imm_size = opnd_size == 2 ? 4 : 6;
    else /* because GCC is stupid */
        __builtin_abort();
    ptr += imm_size;

    ctx->base.ptr = ptr;
    ctx->base.newop_size = ctx->base.op_size = ptr - orig;
    /* printf("bits=%x\n", bits); */

    if (bits & I_JIMM_ONLY) {
        int32_t imm;
        const void *imm_ptr = orig + imm_off;
        switch (imm_size) {
            case 1: imm = *(int8_t *)  imm_ptr; break;
            case 2: imm = *(int16_t *) imm_ptr; break;
            case 4: imm = *(int32_t *) imm_ptr; break;
            default: __builtin_abort();
        }

        bool cond = !(byte1 == 0xe2 || (byte1 >= 0xe8 && byte1 <= 0xeb));
        bool call = !(bits & I_JMP);
        P(branch)(ctx, ctx->base.pc + ctx->base.op_size + imm,
                  cond * CC_CONDITIONAL | call * CC_CALL);
        if (DIS_MAY_MODIFY && ctx->base.modify) {
            /* newval[0] should be the new immediate */
            int32_t new_imm = ctx->base.newval[0];
            uint8_t *new_op = ctx->base.newop;
            memcpy(new_op, orig, ctx->base.op_size);
            uint8_t *new_imm_ptr = new_op + imm_off;
            switch (imm_size) {
                case 1: *(int8_t *)  new_imm_ptr = new_imm; break;
                case 2: *(int16_t *) new_imm_ptr = new_imm; break;
                case 4: *(int32_t *) new_imm_ptr = new_imm; break;
                default: __builtin_abort();
            }
        }
#ifdef TARGET_x86_64
    } else if ((bits & I_MODA) == I_MODA && mod == 0 && rm == 5) {
        int32_t disp = *(int32_t *) (orig + modrm_off + 1);
        if (bits & I_CALL)
            P(indirect_call)(ctx);
        /* unlike ARM, we can always switch to non-pcrel without making the
         * instruction from scratch, so we don't have 'reg' and 'lm' */
        struct arch_pcrel_info info = {
            .reg = modrm >> 3 & 7,
            .is_jump = !!(bits & I_JMP),
        };
        P(pcrel)(ctx, ctx->base.pc + ctx->base.op_size + disp, info);
        if (DIS_MAY_MODIFY && ctx->base.modify) {
            uint8_t *new_op = ctx->base.newop;
            memcpy(new_op, orig, ctx->base.op_size);
            /* newval[0] should be the new register;
             * newval[1] should be the new displacement */
            int new_reg = ctx->base.newval[0];
            uint32_t new_disp = ctx->base.newval[1];
            uint8_t *new_modrm_ptr = new_op + modrm_off;

            int new_disp_size = new_disp ? 1 : 0;

            *new_modrm_ptr = (*new_modrm_ptr & ~0xc7) |
                             (new_disp_size ? 1 : 0) << 6 |
                             ctx->base.newval[0];
            uint8_t *memspec_end = new_modrm_ptr + 1;
            if (new_reg == 4) {
                /* rsp - need SIB */
                *memspec_end++ = 0x24;
            }
            if (new_disp_size)
                *memspec_end++ = new_disp;

            memmove(memspec_end, new_modrm_ptr + 5,
                    ctx->base.op_size - modrm_off - 1);
            ctx->base.newop_size -= 5 - (memspec_end - new_modrm_ptr);
        }
#endif
    } else if ((bits & I_TYPE_MASK) == I_JMP) {
        P(ret)(ctx);
    } else if (bits & I_CALL) {
        P(indirect_call)(ctx);
    } else {
        P(unidentified)(ctx);
    }
}
