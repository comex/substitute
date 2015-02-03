static INLINE void P(adrlabel_label_unk_Xd_1_ADR)(tdis_ctx ctx, struct bitslice Xd, struct bitslice label) {
    return P(pcrel)(ctx, ctx->pc + sext(bs_get(label, ctx->op), 22),
                    bs_get(Xd, ctx->op), PLM_ADR);
}
static INLINE void P(adrplabel_label_unk_Xd_1_ADRP)(tdis_ctx ctx, struct bitslice Xd, struct bitslice label) {
    return P(pcrel)(ctx, ctx->pc + (sext(bs_get(label, ctx->op), 22) << 12),
                    bs_get(Xd, ctx->op), PLM_ADR);
}
static INLINE void P(am_b_target_addr_B_1_B)(tdis_ctx ctx, struct bitslice addr) {
    return P(branch)(ctx, ctx->pc + sext(bs_get(addr, ctx->op), 26) * 4,
                     /*cc*/ 0);
}
static INLINE void P(am_bl_target_addr_1_BL)(tdis_ctx ctx, struct bitslice addr) {
    return P(branch)(ctx, ctx->pc + sext(bs_get(addr, ctx->op), 26) * 4,
                     /*cc*/ 0);
}
static INLINE void P(ccode_cond_am_brcond_target_B_1_Bcc)(tdis_ctx ctx, struct bitslice cond, struct bitslice target) {
    int bits = bs_get(cond, ctx->op);
    /* Bcc with AL/NV (which is actually just another AL) is useless but possible. */
    int cc = bits >= 0xe ? 0 : (CC_ARMCC | bits);
    return P(branch)(ctx, ctx->pc + sext(bs_get(target, ctx->op), 19) * 4, cc);
}
static INLINE void P(am_tbrcond_target_B_4_TBNZW)(tdis_ctx ctx, struct bitslice target) {
    P(branch)(ctx, ctx->pc + sext(bs_get(target, ctx->op), 14) * 4, CC_XBXZ);
    if (TDIS_CTX_MODIFY(ctx)) {
        /* ditto CBNZ on ARM */
        int new_target = (TDIS_CTX_NEWVAL(ctx, 0) - ctx->pc) / 4;
        unsigned new = bs_set(target, new_target, ctx->op);
        if (TDIS_CTX_NEWVAL(ctx, 1))
            new ^= 1 << 24;
        TDIS_CTX_SET_NEWOP(ctx, new);
    }
}
static INLINE void P(am_brcond_target_B_4_CBNZW)(tdis_ctx ctx, struct bitslice target) {
    /* both have the same bit to control Z/NZ */
    return P(am_tbrcond_target_B_4_TBNZW)(ctx, target);
}
static INLINE void P(am_ldrlit_label_unk_Rt_6_LDRDl)(tdis_ctx ctx, struct bitslice Rt, struct bitslice label) {
    enum pcrel_load_mode mode;
    if ((ctx->op >> 26) & 1) {
        switch (ctx->op >> 30) {
            case 0: mode = PLM_U32_SIMD; break;
            case 1: mode = PLM_U64_SIMD; break;
            case 2: mode = PLM_U128_SIMD; break;
            default: __builtin_abort();
        }
    } else {
        switch (ctx->op >> 30) {
            case 0: mode = PLM_U32; break;
            case 1: mode = PLM_U64; break;
            case 2: mode = PLM_S32; break;
            default: __builtin_abort();
        }
    }
    return P(pcrel)(ctx, ctx->pc + sext(bs_get(label, ctx->op), 19) * 4,
                    bs_get(Rt, ctx->op), mode);
}
static INLINE void P(GPR64_Rn_1_RET)(tdis_ctx ctx, UNUSED struct bitslice Rn) {
    return P(ret)(ctx);
}

static INLINE void P(dis)(tdis_ctx ctx) {
    uint32_t op = ctx->op = *(uint32_t *) ctx->ptr;
    ctx->op_size = 4;
    /* clang doesn't realize that this is unreachable and generates code like
     * "and ecx, 0x1f; cmp ecx, 0x1f; ja abort".  Yeah, nice job there. */
    #include "../generated/generic-dis-arm64.inc.h"
    __builtin_abort();
}
