static INLINE tdis_ret P(adrlabel_label_unk_Xd_1_ADR)(tdis_ctx ctx, struct bitslice Xd, struct bitslice label) {
    return P(pcrel)(ctx, ctx->pc + sext(bs_get(label, ctx->op), 22), bs_get(Xd, ctx->op), false);
}
static INLINE tdis_ret P(adrplabel_label_unk_Xd_1_ADRP)(tdis_ctx ctx, struct bitslice Xd, struct bitslice label) {
    return P(pcrel)(ctx, ctx->pc + (sext(bs_get(label, ctx->op), 22) << 12), bs_get(Xd, ctx->op), false);
}
static INLINE tdis_ret P(am_b_target_addr_B_1_B)(tdis_ctx ctx, struct bitslice addr) {
    return P(branch)(ctx, ctx->pc + sext(bs_get(addr, ctx->op), 26) * 4);
}
static INLINE tdis_ret P(am_bl_target_addr_1_BL)(tdis_ctx ctx, struct bitslice addr) {
    return P(branch)(ctx, ctx->pc + sext(bs_get(addr, ctx->op), 26) * 4);
}
static INLINE tdis_ret P(am_brcond_target_B_5_Bcc)(tdis_ctx ctx, struct bitslice target) {
    return P(branch)(ctx, ctx->pc + sext(bs_get(target, ctx->op), 19) * 4);
}
static INLINE tdis_ret P(am_ldrlit_label_unk_Rt_6_LDRDl)(tdis_ctx ctx, struct bitslice Rt, struct bitslice label) {
    return P(pcrel)(ctx, ctx->pc + sext(bs_get(label, ctx->op), 19) * 4, bs_get(Rt, ctx->op), true);
}
static INLINE tdis_ret P(am_tbrcond_target_B_4_TBNZW)(tdis_ctx ctx, struct bitslice target) {
    return P(branch)(ctx, ctx->pc + sext(bs_get(target, ctx->op), 14) * 4);
}
static INLINE tdis_ret P(GPR64_Rn_1_RET)(tdis_ctx ctx, UNUSED struct bitslice Rn) {
    return P(ret)(ctx);
}

static tdis_ret P(dis_arm64)(tdis_ctx ctx) {
    unsigned op = ctx->op;
    #include "../generated/transform-dis-arm64.inc.h"
    __builtin_abort();
}
