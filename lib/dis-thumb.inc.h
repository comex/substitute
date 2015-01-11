static inline tdis_ret transform_dis_thumb_GPR_Rm_3_tADDhirr(tdis_ctx ctx, struct bitslice Rm) {
    return P(regs)(ctx, Rm, 0, 3, Rm, 0, 0);
}
static inline tdis_ret transform_dis_thumb_GPR_Rm_B_1_tBX(tdis_ctx ctx, struct bitslice Rm) {
    if(bs_get(Rm, ctx->op) == 15) // bx pc?
        return P(bad)(ctx);
    return P(unidentified)(ctx);
}
static inline tdis_ret transform_dis_thumb_t_addrmode_pc_addr_unk_Rt_1_tLDRpci(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    return P(adr)(ctx, ((ctx->pc + 4) & ~2) + bs_get(addr, ctx->op));
}
static inline tdis_ret transform_dis_thumb_t_adrlabel_addr_1_tADR(tdis_ctx ctx, struct bitslice addr) {
    return P(adr)(ctx, ((ctx->pc + 4) & ~2) + bs_get(addr, ctx->op));
}
static inline tdis_ret transform_dis_thumb_t_brtarget_target_B_1_tB(tdis_ctx ctx, struct bitslice target) {
    return P(branch)(ctx->pc + 2 + sext(bs_get(target, ctx->op), 11);
}
static inline tdis_ret transform_dis_thumb_t_bcctarget_target_B_1_tBcc(tdis_ctx ctx, struct bitslice target) {
    return P(branch)(ctx->pc + 2 + sext(bs_get(target, ctx->op), 8);
}
static inline tdis_ret transform_dis_thumb_t_cbtarget_target_B_2_tCBNZ(tdis_ctx ctx, struct bitslice target) {
    return P(branch)(ctx->pc + 2 + bs_get(target, ctx->op);
}

static inline tdis_ret P(dis_thumb)(tdis_ctx ctx) {
    unsigned op = ctx->op;
    #include "../generated/transform-dis-thumb.inc"
}
