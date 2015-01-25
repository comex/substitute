#pragma once
#include "dis.h"

struct assemble_ctx {
    void **codep;
    bool thumb;
};

static inline void PUSHone(struct assemble_ctx ctx, int Rt) {
    if (ctx.thumb)
        op32(ctx.codep, 0x0d04f84d | Rt << 28);
    else
        op32(ctx.codep, 0xe52d0004 | Rt << 12);
}

static inline void POPone(struct assemble_ctx ctx, int Rt) {
    if (ctx.thumb)
        op32(ctx.codep, 0x0b04f85d | Rt << 28);
    else
        op32(ctx.codep, 0xe49d0004 | Rt << 12);
}

static inline void POPmulti(struct assemble_ctx ctx, uint16_t mask) {
    if (ctx.thumb)
        op32(ctx.codep, 0x0000e8bd | mask << 16);
    else
        op32(ctx.codep, 0xe8bd0000 | mask);
}

static inline void MOVW_MOVT(struct assemble_ctx ctx, int Rd, uint32_t val) {
    uint16_t hi = val >> 16, lo = (uint16_t) val;
    if (ctx.thumb) {
        op32(ctx.codep, 0x0000f240 | Rd << 24 | lo >> 12 | (lo >> 11 & 1) << 10 |
                        (lo >> 8 & 7) << 28 | (lo & 0xff) << 16);
        op32(ctx.codep, 0x0000f2c0 | Rd << 24 | hi >> 12 | (hi >> 11 & 1) << 10 |
                        (hi >> 8 & 7) << 28 | (hi & 0xff) << 16);

    } else {
        op32(ctx.codep, 0xe3000000 | Rd << 12 | (lo >> 12) << 16 | (lo & 0xfff));
        op32(ctx.codep, 0xe3400000 | Rd << 12 | (hi >> 12) << 16 | (hi & 0xfff));
    }

}

static inline void STRri(struct assemble_ctx ctx, int Rt, int Rn, uint32_t off) {
    if (ctx.thumb)
        op32(ctx.codep, 0x0000f8c0 | Rn | Rt << 28 | off << 16);
    else
        op32(ctx.codep, 0xe4800000 | Rn << 16 | Rt << 12 | off);
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
                op32(ctx.codep, 0xe1c00000 | Rn << 16 | Rt << 12 | subop << 4 |
                                (off & 0xf) | (off & 0xf0) << 4 | not_ldrd << 20);
                break;
            default:
                __builtin_abort();
        }
    }
}
