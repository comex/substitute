#pragma once
#include "dis.h"

struct assemble_ctx {
    void **codep;
    uint_tptr pc;
    bool thumb;
    int cond;
};

static inline void PUSHone(struct assemble_ctx ctx, int Rt) {
    if (ctx.thumb)
        op32(ctx.codep, 0x0d04f84d | Rt << 28);
    else
        op32(ctx.codep, 0x052d0004 | Rt << 12 | ctx.cond << 28);
}

static inline void PUSHmulti(struct assemble_ctx ctx, uint16_t mask) {
    if (ctx.thumb)
        op32(ctx.codep, 0x0000e8ad | mask << 16);
    else
        op32(ctx.codep, 0x092d0000 | mask | ctx.cond << 28);
}

static inline void POPone(struct assemble_ctx ctx, int Rt) {
    if (ctx.thumb)
        op32(ctx.codep, 0x0b04f85d | Rt << 28);
    else
        op32(ctx.codep, 0x049d0004 | Rt << 12 | ctx.cond << 28);
}

static inline void POPmulti(struct assemble_ctx ctx, uint16_t mask) {
    if (ctx.thumb)
        op32(ctx.codep, 0x0000e8bd | mask << 16);
    else
        op32(ctx.codep, 0x08bd0000 | mask | ctx.cond << 28);
}

static inline void MOVW_MOVT(struct assemble_ctx ctx, int Rd, uint32_t val) {
    uint16_t hi = val >> 16, lo = (uint16_t) val;
    if (ctx.thumb) {
        op32(ctx.codep, 0x0000f240 | Rd << 24 | lo >> 12 | (lo >> 11 & 1) << 10 |
                        (lo >> 8 & 7) << 28 | (lo & 0xff) << 16);
        op32(ctx.codep, 0x0000f2c0 | Rd << 24 | hi >> 12 | (hi >> 11 & 1) << 10 |
                        (hi >> 8 & 7) << 28 | (hi & 0xff) << 16);

    } else {
        op32(ctx.codep, 0x03000000 | Rd << 12 | (lo >> 12) << 16 | (lo & 0xfff) |
                        ctx.cond << 28);
        op32(ctx.codep, 0x03400000 | Rd << 12 | (hi >> 12) << 16 | (hi & 0xfff) |
                        ctx.cond << 28);
    }

}

static inline void STRri(struct assemble_ctx ctx, int Rt, int Rn, uint32_t off) {
    if (ctx.thumb)
        op32(ctx.codep, 0x0000f8c0 | Rn | Rt << 28 | off << 16);
    else
        op32(ctx.codep, 0x04800000 | Rn << 16 | Rt << 12 | off | ctx.cond << 28);
}

static inline void LDRxi(struct assemble_ctx ctx, int Rt, int Rn, uint32_t off,
                         enum pcrel_load_mode load_mode) {
    if (ctx.thumb) {
        int subop, sign;
        switch (load_mode) {
            case PLM_U8:  subop = 0; sign = 0; break;
            case PLM_S8:  subop = 0; sign = 1; break;
            case PLM_U16: subop = 1; sign = 0; break;
            case PLM_S16: subop = 1; sign = 1; break;
            case PLM_U32: subop = 2; sign = 0; break;
            default: __builtin_abort();
        }
        op32(ctx.codep, 0x0000f890 | Rn | Rt << 28 | subop << 5 | sign << 8 |
                        off << 16);
    } else {
        int is_byte, subop, not_ldrd;
        switch (load_mode) {
            case PLM_U8:   is_byte = 1; goto type1;
            case PLM_S8:   subop = 13; not_ldrd = 1; goto type2;
            case PLM_U16:  subop = 11; not_ldrd = 1; goto type2;
            case PLM_S16:  subop = 15; not_ldrd = 1; goto type2;
            case PLM_U32:  is_byte = 0; goto type1;
            case PLM_U128: subop = 13; not_ldrd = 0; goto type2;
            type1:
                op32(ctx.codep, 0xe5900000 | Rn << 16 | Rt << 12 | off);
                break;
            type2:
                op32(ctx.codep, 0x01c00000 | Rn << 16 | Rt << 12 | subop << 4 |
                                (off & 0xf) | (off & 0xf0) << 4 | not_ldrd << 20 |
                                ctx.cond << 28);
                break;
            default:
                __builtin_abort();
        }
    }
}

static inline void Bccrel(struct assemble_ctx ctx, int offset) {
    if (ctx.thumb) {
        offset = (offset - 4) / 2;
        op16(ctx.codep, 0xd000 | ctx.cond << 8 | offset);
    } else {
        offset = (offset - 8) / 4;
        op32(ctx.codep, 0x0a000000 | offset | ctx.cond << 28);
    }
}

static inline void LDR_PC(struct assemble_ctx ctx, uint32_t dpc) {
    if (ctx.pc & 2)
        op16(ctx.codep, 0xbf00);
    if (ctx.thumb)
        op32(ctx.codep, 0xf000f8df);
    else
        op32(ctx.codep, 0x051ff004 | ctx.cond << 28);
    op32(ctx.codep, (uint32_t) dpc);
}

static inline void ADD_PC(struct assemble_ctx ctx, uint32_t Rd, uint32_t imm12) {
    if (ctx.thumb)
        op32(ctx.codep, 0x0000f20f | ((imm12 >> 11) << 10) | (((imm12 & 0x700) >> 8) << 28) | Rd << 24 | (imm12 & 0xff) << 16);
    else
        op32(ctx.codep, 0x028f0000 | Rd << 12 | imm12 << 0 | ctx.cond << 28);
}
