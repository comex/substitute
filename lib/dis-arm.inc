static inline tdis_ret P(addr_offset_none_addr_33_STC2L_OPTION)(tdis_ctx ctx, struct bitslice addr) {}
static inline tdis_ret P(GPR_Rn_GPR_Rm_1_ADDrr)(tdis_ctx ctx, struct bitslice Rm, struct bitslice Rn) {

    return P(regs)(ctx, Rm, 0, 4, Rn, 0, 4);
}
static inline tdis_ret P(GPR_Rn_3_ADDri)(tdis_ctx ctx, struct bitslice Rn) {
    return P(regs)(ctx, Rn, 0, 4, Rn, 0, 0);
}
static inline tdis_ret P(addrmode3_addr_2_STRD)(tdis_ctx ctx, struct bitslice addr) {
    return P(regs)(ctx, addr, 9, 4, addr, 0, 4);
}
static inline tdis_ret P(addrmode3_pre_addr_2_STRD_PRE)(tdis_ctx ctx, struct bitslice addr) {
    return P(regs)(ctx, addr, 9, 4, addr, 0, 4);
}
static inline tdis_ret P(addrmode_imm12_addr_2_STRBi12)(tdis_ctx ctx, struct bitslice addr) {
    return P(regs)(ctx, addr, 13, 4, addr, 0, 0);
}
static inline tdis_ret P(addrmode_imm12_pre_addr_2_STRB_PRE_IMM)(tdis_ctx ctx, struct bitslice addr) {
    return P(regs)(ctx, addr, 13, 4, addr, 0, 0);
}
static inline tdis_ret P(addrmode5_addr_8_STC2L_OFFSET)(tdis_ctx ctx, struct bitslice addr) {
    return P(regs)(ctx, addr, 9, 4, addr, 0, 0);
}
static inline tdis_ret P(addrmode5_pre_addr_4_STC2L_PRE)(tdis_ctx ctx, struct bitslice addr) {
    return P(regs)(ctx, addr, 9, 4, addr, 0, 0);
}
static inline tdis_ret P(adrlabel_label_1_ADR)(tdis_ctx ctx, struct bitslice label) {
    return P(adr)(ctx, ctx->pc + 8 + bs_get(label, ctx->op));
}
static inline tdis_ret P(br_target_target_1_Bcc)(tdis_ctx ctx, struct bitslice target) {
    return P(branch)(ctx, ctx->pc + 8 + sext(bs_get(target, ctx->op), 24));
}
static inline tdis_ret P(ldst_so_reg_addr_2_STRB_PRE_REG)(tdis_ctx ctx, struct bitslice addr) {
    return P(regs)(ctx, addr, 13, 4, addr, 0, 4);
}

