
static inline tdis_ret P(GPR_Rt_addr_offset_none_addr_S_21_STL)(tdis_ctx ctx, struct bitslice Rt, struct bitslice addr) {
}
static inline tdis_ret P(GPR_Rn_GPR_Rm_1_ADDrr)(tdis_ctx ctx, struct bitslice Rm, struct bitslice Rn) {
}
static inline tdis_ret P(GPR_Rn_3_ADDri)(tdis_ctx ctx, struct bitslice Rn) {
}
static inline tdis_ret P(addr_offset_none_addr_unk_Rt_2_SWP)(tdis_ctx ctx, struct bitslice Rt, struct bitslice addr) {
}
static inline tdis_ret P(GPRPairOp_Rt_addr_offset_none_addr_2_STLEXD)(tdis_ctx ctx, struct bitslice Rt, struct bitslice addr) {
    return P(reg)(ctx, addr, 0, 4);
}
static inline tdis_ret P(GPR_Rt_addrmode3_addr_S_2_STRD)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    return P(regs3)(ctx, addr, 9, 4, addr, 0, 4, Rt, 0, 4);
}
static inline tdis_ret P(GPR_Rt_addrmode3_pre_addr_S_2_STRD_PRE)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    return P(regs3)(ctx, addr, 9, 4, addr, 0, 4, Rt, 0, 4);
}
static inline tdis_ret P(adrlabel_label_1_ADR)(tdis_ctx ctx, struct bitslice label) {
    return P(adr)(ctx, ctx->pc + 8 + bs_get(label, ctx->op));
}
static inline tdis_ret P(br_target_target_B_1_Bcc)(tdis_ctx ctx, struct bitslice target) {
    return P(branch)(ctx, ctx->pc + 8 + sext(bs_get(target, ctx->op), 24));
}
static inline tdis_ret P(GPR_Rt_S_1_STRrs)(tdis_ctx ctx, struct bitslice Rt) {
}
static inline tdis_ret P(GPRnopc_Rt_S_1_STRBrs)(tdis_ctx ctx, struct bitslice Rt) {
}
static inline tdis_ret P(unk_Rt_11_VMOVRRD)(tdis_ctx ctx, struct bitslice Rt) {}
static inline tdis_ret P(addr_offset_none_addr_8_STC2L_OPTION)(tdis_ctx ctx, struct bitslice addr) {}
static inline tdis_ret P(GPR_Rt_addrmode_imm12_addr_1_STRi12)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {}
static inline tdis_ret P(addrmode5_addr_S_8_STC2L_OFFSET)(tdis_ctx ctx, struct bitslice addr) {
    return P(reg)(ctx, addr, 9, 4);
}
static inline tdis_ret P(addrmode5_pre_addr_S_4_STC2L_PRE)(tdis_ctx ctx, struct bitslice addr) {
    return P(reg)(ctx, addr, 9, 4);
}
static inline tdis_ret P(GPRnopc_Rt_addrmode_imm12_addr_1_STRBi12)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    return P(reg)(ctx, addr, 13, 4);
}
static inline tdis_ret P(GPR_Rt_addrmode_imm12_pre_addr_2_STRB_PRE_IMM)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    return P(reg)(ctx, addr, 13, 4);
}
static inline tdis_ret P(GPR_Rt_ldst_so_reg_addr_2_STRB_PRE_REG)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    return P(regs2)(ctx, addr, 13, 4, addr, 0, 4);
}


static inline tdis_ret P(so_reg_imm_shift_14_ADCrsi)(tdis_ctx ctx, struct bitslice shift) {}
static inline tdis_ret P(so_reg_reg_shift_14_ADCrsr)(tdis_ctx ctx, struct bitslice shift) {}
static inline tdis_ret P(addr_offset_none_addr_unk_Rt_31_LDA)(tdis_ctx ctx, struct bitslice Rt, struct bitslice addr) {
    /* hope nobody's SWPping into PC */
    return P(reg)(ctx, addr, 0, 4);
}
static inline tdis_ret P(GPR_Rt_addr_offset_none_addr_S_21_STL)(tdis_ctx ctx, struct bitslice Rt, struct bitslice addr) {
    return P(regs)(ctx, addr, 0, 4, addr, 0, 0, &Rt);
}
static inline tdis_ret P(adrlabel_label_1_ADR)(tdis_ctx ctx, struct bitslice label) {}
static inline tdis_ret P(br_target_target_B_1_Bcc)(tdis_ctx ctx, struct bitslice target) {}
static inline tdis_ret P(GPR_Rt_4_MCR)(tdis_ctx ctx, struct bitslice Rt) {}
static inline tdis_ret P(GPRnopc_Rt_4_MCRR)(tdis_ctx ctx, struct bitslice Rt) {}
static inline tdis_ret P(unk_Rt_13_MRC)(tdis_ctx ctx, struct bitslice Rt) {}
static inline tdis_ret P(addr_offset_none_addr_S_8_STC2L_OPTION)(tdis_ctx ctx, struct bitslice addr) {}
static inline tdis_ret P(addr_offset_none_addr_8_LDC2L_OPTION)(tdis_ctx ctx, struct bitslice addr) {}
static inline tdis_ret P(GPR_Rn_GPR_Rm_1_ADDrr)(tdis_ctx ctx, struct bitslice Rm, struct bitslice Rn) {
    return P(regs2)(ctx, Rm, 0, 4, Rn, 0, 4);
}
static inline tdis_ret P(GPR_Rn_so_reg_imm_shift_1_ADDrsi)(tdis_ctx ctx, struct bitslice shift, struct bitslice Rn) {}
static inline tdis_ret P(GPR_Rn_so_reg_reg_shift_1_ADDrsr)(tdis_ctx ctx, struct bitslice shift, struct bitslice Rn) {}
static inline tdis_ret P(GPR_Rn_1_ADDri)(tdis_ctx ctx, struct bitslice Rn) {
    return P(reg)(ctx, Rn, 0, 4);
}
static inline tdis_ret P(addrmode3_addr_unk_Rt_4_LDRD)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {}
static inline tdis_ret P(GPR_Rt_addrmode3_addr_S_2_STRD)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {}
static inline tdis_ret P(GPR_Rt_addrmode_imm12_addr_S_1_STRi12)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {}
static inline tdis_ret P(GPR_Rt_ldst_so_reg_shift_S_1_STRrs)(tdis_ctx ctx, struct bitslice shift, struct bitslice Rt) {}
static inline tdis_ret P(addrmode5_addr_8_LDC2L_OFFSET)(tdis_ctx ctx, struct bitslice addr) {}
static inline tdis_ret P(addrmode3_pre_addr_unk_Rt_4_LDRD_PRE)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {}
static inline tdis_ret P(GPR_Rt_addrmode3_pre_addr_S_2_STRD_PRE)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {}
static inline tdis_ret P(GPRnopc_Rt_addrmode_imm12_addr_S_1_STRBi12)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {}
static inline tdis_ret P(GPRnopc_Rt_ldst_so_reg_shift_S_1_STRBrs)(tdis_ctx ctx, struct bitslice shift, struct bitslice Rt) {}
static inline tdis_ret P(addrmode5_addr_S_4_STC2L_OFFSET)(tdis_ctx ctx, struct bitslice addr) {}
static inline tdis_ret P(addrmode_imm12_addr_unk_Rt_2_LDRBi12)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {}
static inline tdis_ret P(ldst_so_reg_shift_unk_Rt_2_LDRBrs)(tdis_ctx ctx, struct bitslice shift, struct bitslice Rt) {}
static inline tdis_ret P(GPR_Rt_addrmode_imm12_pre_addr_S_2_STRB_PRE_IMM)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {}
static inline tdis_ret P(addrmode5_pre_addr_S_4_STC2L_PRE)(tdis_ctx ctx, struct bitslice addr) {}
static inline tdis_ret P(GPR_Rt_ldst_so_reg_addr_S_2_STRB_PRE_REG)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {}
static inline tdis_ret P(addrmode_imm12_pre_addr_unk_Rt_2_LDRB_PRE_IMM)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {}
static inline tdis_ret P(ldst_so_reg_addr_unk_Rt_2_LDRB_PRE_REG)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {}
static inline tdis_ret P(addrmode5_pre_addr_4_LDC2L_PRE)(tdis_ctx ctx, struct bitslice addr) {}
static inline tdis_ret P(GPR_Rm_1_MOVr)(tdis_ctx ctx, struct bitslice Rm) {}
static inline tdis_ret P(tcGPR_Rm_1_MOVr_TC)(tdis_ctx ctx, struct bitslice Rm) {}
static inline tdis_ret P(GPRPairOp_Rt_addr_offset_none_addr_S_2_STLEXD)(tdis_ctx ctx, struct bitslice Rt, struct bitslice addr) {}

static inline tdis_ret P(dis_arm)(tdis_ctx ctx) {
    unsigned op = ctx->op;
    #include "../generated/transform-dis-arm.inc"
}
