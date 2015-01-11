#include "dis.h"

#define r(nn)           {.n = nn, .out = false}
#define rs(nn, l, s)    {.n = bs_slice(nn, l, s), .out = false}
#define rout(nn)        {.n = nn, .out = true}
#define rsout(nn, l, s) {.n = bs_slice(nn, l, s), .out = true}
#define data(...) \
    struct dis_data_operand ops[] = {__VA_ARGS__}; \
    return P(data)(ctx, ops, sizeof(ops)/sizeof(*ops));

static tdis_ret P(GPRPairOp_Rt_addr_offset_none_addr_S_2_STLEXD)(tdis_ctx ctx, struct bitslice Rt, struct bitslice addr) {
    data(r(Rt), r(addr));
}
static tdis_ret P(GPR_Rm_unk_Rd_1_MOVr)(tdis_ctx ctx, struct bitslice Rm, struct bitslice Rd) {
    data(rout(Rd), r(Rm));
}
static tdis_ret P(GPR_Rn_GPR_Rm_unk_Rd_1_ADDrr)(tdis_ctx ctx, struct bitslice Rm, struct bitslice Rd, struct bitslice Rn) {
    data(rout(Rd), r(Rm), r(Rn));
}
static tdis_ret P(GPR_Rn_so_reg_imm_shift_unk_Rd_1_ADDrsi)(tdis_ctx ctx, struct bitslice shift, struct bitslice Rd, struct bitslice Rn) {
    data(rs(shift, 0, 4), r(Rn), rout(Rd));
}
static tdis_ret P(GPR_Rn_so_reg_reg_shift_unk_Rd_1_ADDrsr)(tdis_ctx ctx, struct bitslice shift, struct bitslice Rd, struct bitslice Rn) {
    data(rs(shift, 0, 4), rs(shift, 8, 4), r(Rn), rout(Rd));
}
static tdis_ret P(GPR_Rn_unk_Rd_1_ADDri)(tdis_ctx ctx, struct bitslice Rd, struct bitslice Rn) {
    data(rout(Rd), r(Rn));
}
static tdis_ret P(GPR_Rt_4_MCR)(tdis_ctx ctx, struct bitslice Rt) {
    data(r(Rt));
}
static tdis_ret P(GPR_Rt_addr_offset_none_addr_S_10_STL)(tdis_ctx ctx, struct bitslice Rt, struct bitslice addr) {
    data(r(addr), rout(Rt));
}
static tdis_ret P(GPR_Rt_addr_offset_none_addr_am2offset_imm_offset_S_4_STRBT_POST_IMM)(tdis_ctx ctx, UNUSED struct bitslice offset, struct bitslice Rt, struct bitslice addr) {
    data(r(addr), r(Rt));
}
static tdis_ret P(GPR_Rt_addr_offset_none_addr_am2offset_reg_offset_S_4_STRBT_POST_REG)(tdis_ctx ctx, struct bitslice offset, struct bitslice Rt, struct bitslice addr) {
    data(r(addr), rs(offset, 0, 4), r(Rt));
}
static tdis_ret P(GPR_Rt_addr_offset_none_addr_am3offset_offset_S_2_STRD_POST)(tdis_ctx ctx, struct bitslice offset, struct bitslice Rt, struct bitslice addr) {
    data(r(addr), rs(offset, 0, 4), r(Rt));
}
static tdis_ret P(GPR_Rt_addr_offset_none_addr_postidx_imm8_offset_S_1_STRHTi)(tdis_ctx ctx, UNUSED struct bitslice offset, struct bitslice Rt, struct bitslice addr) {
    data(r(addr), r(Rt));
}
static tdis_ret P(GPR_Rt_addrmode3_addr_S_2_STRD)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rs(addr, 9, 4), rs(addr, 0, 4), r(Rt));
}
static tdis_ret P(GPR_Rt_addrmode3_pre_addr_S_2_STRD_PRE)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rs(addr, 9, 4), rs(addr, 0, 4), r(Rt));
}
static tdis_ret P(GPR_Rt_addrmode_imm12_addr_S_1_STRi12)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rs(addr, 13, 4), r(Rt));
}
static tdis_ret P(GPR_Rt_addrmode_imm12_pre_addr_S_2_STRB_PRE_IMM)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rs(addr, 13, 4), r(Rt));
}
static tdis_ret P(GPR_Rt_ldst_so_reg_addr_S_2_STRB_PRE_REG)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rs(addr, 0, 4), rs(addr, 13, 4), r(Rt));
}
static tdis_ret P(GPR_Rt_ldst_so_reg_shift_S_1_STRrs)(tdis_ctx ctx, struct bitslice shift, struct bitslice Rt) {
    data(rs(shift, 0, 4), rs(shift, 13, 4), r(Rt));
}
static tdis_ret P(GPRnopc_Rt_4_MCRR)(tdis_ctx ctx, UNUSED struct bitslice Rt) {
    // need Rt2 but whatever
    return P(unidentified)(ctx);
}
static tdis_ret P(GPRnopc_Rt_addrmode_imm12_addr_S_1_STRBi12)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rs(addr, 13, 4), r(Rt));
}
static tdis_ret P(GPRnopc_Rt_ldst_so_reg_shift_S_1_STRBrs)(tdis_ctx ctx, struct bitslice shift, struct bitslice Rt) {
    data(rs(shift, 13, 4), rs(shift, 0, 4), r(Rt));
}
static tdis_ret P(addr_offset_none_addr_4_LDC2L_OPTION)(tdis_ctx ctx, struct bitslice addr) {
    data(r(addr));
}
static tdis_ret P(addr_offset_none_addr_S_4_STC2L_OPTION)(tdis_ctx ctx, struct bitslice addr) {
    data(r(addr));
}
static tdis_ret P(addr_offset_none_addr_am2offset_imm_offset_unk_Rt_4_LDRBT_POST_IMM)(tdis_ctx ctx, UNUSED struct bitslice offset, struct bitslice Rt, struct bitslice addr) {
    data(r(addr), rout(Rt));
}
static tdis_ret P(addr_offset_none_addr_am2offset_reg_offset_unk_Rt_4_LDRBT_POST_REG)(tdis_ctx ctx, struct bitslice offset, struct bitslice Rt, struct bitslice addr) {
    data(r(addr), rs(offset, 0, 4), rout(Rt));
}
static tdis_ret P(addr_offset_none_addr_am3offset_offset_unk_Rt_4_LDRD_POST)(tdis_ctx ctx, struct bitslice offset, struct bitslice Rt, struct bitslice addr) {
    data(r(addr), rs(offset, 0, 4), rout(Rt));
}
static tdis_ret P(addr_offset_none_addr_postidx_imm8_offset_unk_Rt_3_LDRHTi)(tdis_ctx ctx, UNUSED struct bitslice offset, struct bitslice Rt, struct bitslice addr) {
    data(r(addr), rout(Rt));
}
static tdis_ret P(addr_offset_none_addr_postidx_imm8s4_offset_4_LDC2L_POST)(tdis_ctx ctx, UNUSED struct bitslice offset, struct bitslice addr) {
    data(r(addr));
}
static tdis_ret P(addr_offset_none_addr_postidx_imm8s4_offset_S_4_STC2L_POST)(tdis_ctx ctx, UNUSED struct bitslice offset, struct bitslice addr) {
    data(r(addr));
}
static tdis_ret P(addr_offset_none_addr_unk_Rt_16_LDA)(tdis_ctx ctx, struct bitslice Rt, struct bitslice addr) {
    data(r(addr), rout(Rt));
}
static tdis_ret P(addrmode3_addr_unk_Rt_4_LDRD)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rsout(addr, 9, 4), rs(addr, 0, 4), r(Rt));
}
static tdis_ret P(addrmode3_pre_addr_unk_Rt_4_LDRD_PRE)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rsout(addr, 9, 4), rs(addr, 0, 4), r(Rt));
}
static tdis_ret P(addrmode5_addr_8_LDC2L_OFFSET)(tdis_ctx ctx, struct bitslice addr) {
    data(rsout(addr, 9, 4));
}
static tdis_ret P(addrmode5_addr_S_4_STC2L_OFFSET)(tdis_ctx ctx, struct bitslice addr) {
    data(rs(addr, 9, 4));
}
static tdis_ret P(addrmode5_pre_addr_4_LDC2L_PRE)(tdis_ctx ctx, struct bitslice addr) {
    data(rs(addr, 9, 4));
}
static tdis_ret P(addrmode5_pre_addr_S_4_STC2L_PRE)(tdis_ctx ctx, struct bitslice addr) {
    data(rs(addr, 9, 4));
}
static tdis_ret P(addrmode_imm12_addr_unk_Rt_2_LDRBi12)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rs(addr, 13, 4), rout(Rt));
}
static tdis_ret P(addrmode_imm12_pre_addr_unk_Rt_2_LDRB_PRE_IMM)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rs(addr, 13, 4), rout(Rt));
}
static tdis_ret P(adrlabel_label_1_ADR)(tdis_ctx ctx, struct bitslice label) {
    return P(adr)(ctx, ctx->pc + 8 + bs_get(label, ctx->op));
}
static tdis_ret P(br_target_target_B_1_Bcc)(tdis_ctx ctx, struct bitslice target) {
    return P(branch)(ctx, ctx->pc + 8 + sext(bs_get(target, ctx->op), 24));
}
static tdis_ret P(ldst_so_reg_addr_unk_Rt_2_LDRB_PRE_REG)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rs(addr, 0, 4), rs(addr, 13, 4), rout(Rt));
}
static tdis_ret P(ldst_so_reg_shift_unk_Rt_2_LDRBrs)(tdis_ctx ctx, struct bitslice shift, struct bitslice Rt) {
    data(rs(shift, 0, 4), rs(shift, 13, 4), rout(Rt));
}
static tdis_ret P(tcGPR_Rm_unk_Rd_1_MOVr_TC)(tdis_ctx ctx, struct bitslice Rm, struct bitslice Rd) {
    data(rout(Rd), r(Rm));
}
static tdis_ret P(unk_Rd_5_MOVTi16)(tdis_ctx ctx, struct bitslice Rd) {
    data(rout(Rd));
}
static inline tdis_ret P(unk_Rt_13_MRC)(tdis_ctx ctx, struct bitslice Rt) {
    data(rout(Rt));
}

static tdis_ret P(dis_arm)(tdis_ctx ctx) {
    unsigned op = ctx->op;
    #include "../generated/transform-dis-arm.inc.h"
}
