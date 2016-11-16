#include "dis.h"

/*
    ARM
           65 24-20
    LDRSB: 10 xx1x1
    LDRH:  01 xx1x1
    LDRSH: 11 xx1x1
    LDRD:  10 xx1x0

    LDRB:  ii 1u101
    LDR:   ii 1u001

    Thumb (such logical)
    LDRB:  11111 00 0 U 00 1 1111
    LDRSB: 11111 00 1 U 00 1 1111
    LDRH:  11111 00 0 U 01 1 1111
    LDRSH: 11111 00 1 U 01 1 1111
    LDR:   11111 00 0 U 10 1 1111
*/

/* TODO: bx lr, and handle conditionals */

static inline enum pcrel_load_mode get_arm_load_mode(unsigned op) {
    if ((op & 0x7000090) == 0x90) {
        return ((op >> 22) & 1) ? PLM_U8 : PLM_U32;
    } else {
        switch ((op >> 4) & 3) {
            default: __builtin_abort();
            case 1: return PLM_U16;
            case 2: return (op & (1 << 20)) ? PLM_S8 : PLM_U128;
            case 3: return PLM_S16;
        }
    }
}

static INLINE void P(GPRPairOp_Rt_addr_offset_none_addr_unk_Rd_S_2_STLEXD)(tdis_ctx ctx, struct bitslice Rt, struct bitslice Rd, struct bitslice addr) {
    data(r(Rt), r(Rd), r(addr));
}
static INLINE void P(GPR_Rm_unk_Rd_1_MOVr)(tdis_ctx ctx, struct bitslice Rm, struct bitslice Rd) {
    data(rout(Rd), r(Rm));
}
static INLINE void P(GPR_Rn_GPR_Rm_unk_Rd_1_ADDrr)(tdis_ctx ctx, struct bitslice Rm, struct bitslice Rd, struct bitslice Rn) {
    data(rout(Rd), r(Rm), r(Rn));
}
static INLINE void P(GPR_Rn_so_reg_imm_shift_unk_Rd_1_ADDrsi)(tdis_ctx ctx, struct bitslice shift, struct bitslice Rd, struct bitslice Rn) {
    data(rout(Rd), rs(shift, 0, 4), r(Rn));
}
static INLINE void P(GPR_Rn_so_reg_reg_shift_unk_Rd_1_ADDrsr)(tdis_ctx ctx, struct bitslice shift, struct bitslice Rd, struct bitslice Rn) {
    data(rout(Rd), rs(shift, 0, 4), rs(shift, 8, 4), r(Rn));
}
static INLINE void P(GPR_Rn_unk_Rd_1_ADDri)(tdis_ctx ctx, struct bitslice Rd, struct bitslice Rn) {
    data(rout(Rd), r(Rn));
}
static INLINE void P(GPR_Rt_4_MCR)(tdis_ctx ctx, struct bitslice Rt) {
    data(r(Rt));
}
static INLINE void P(GPR_Rt_addr_offset_none_addr_S_3_STL)(tdis_ctx ctx, struct bitslice Rt, struct bitslice addr) {
    data(rout(Rt), r(addr));
}
static INLINE void P(GPR_Rt_addr_offset_none_addr_am2offset_imm_offset_S_4_STRBT_POST_IMM)(tdis_ctx ctx, UNUSED struct bitslice offset, struct bitslice Rt, struct bitslice addr) {
    data(r(addr), r(Rt));
}
static INLINE void P(GPR_Rt_addr_offset_none_addr_am2offset_reg_offset_S_4_STRBT_POST_REG)(tdis_ctx ctx, struct bitslice offset, struct bitslice Rt, struct bitslice addr) {
    data(r(addr), rs(offset, 0, 4), r(Rt));
}
static INLINE void P(GPR_Rt_addr_offset_none_addr_am3offset_offset_S_2_STRD_POST)(tdis_ctx ctx, struct bitslice offset, struct bitslice Rt, struct bitslice addr) {
    data_flags(DFLAG_IS_LDRD_STRD, r(Rt), r(addr), rs(offset, 0, 4));
}
static INLINE void P(GPR_Rt_addr_offset_none_addr_postidx_imm8_offset_S_1_STRHTi)(tdis_ctx ctx, UNUSED struct bitslice offset, struct bitslice Rt, struct bitslice addr) {
    data(r(addr), r(Rt));
}
static INLINE void P(GPR_Rt_addrmode3_addr_S_2_STRD)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    unsigned addr_val = bs_get(addr, ctx->base.op);
    if (addr_val & 1 << 13)
        data_flags(DFLAG_IS_LDRD_STRD, r(Rt), rs(addr, 9, 4));
    else
        data_flags(DFLAG_IS_LDRD_STRD, r(Rt), rs(addr, 9, 4), rs(addr, 0, 4));
}
static INLINE void P(GPR_Rt_addrmode3_pre_addr_S_2_STRD_PRE)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    return P(GPR_Rt_addrmode3_addr_S_2_STRD)(ctx, addr, Rt);
}
static INLINE void P(GPR_Rt_addrmode_imm12_addr_S_1_STRi12)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rs(addr, 13, 4), r(Rt));
}
static INLINE void P(GPR_Rt_addrmode_imm12_pre_addr_S_2_STRB_PRE_IMM)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rs(addr, 13, 4), r(Rt));
}
static INLINE void P(GPR_Rt_ldst_so_reg_addr_S_2_STRB_PRE_REG)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rs(addr, 0, 4), rs(addr, 13, 4), r(Rt));
}
static INLINE void P(GPR_Rt_ldst_so_reg_shift_S_1_STRrs)(tdis_ctx ctx, struct bitslice shift, struct bitslice Rt) {
    data(rs(shift, 0, 4), rs(shift, 13, 4), r(Rt));
}
static INLINE void P(GPRnopc_Rt_4_MCRR)(tdis_ctx ctx, UNUSED struct bitslice Rt) {
    /* need Rt2 but whatever */
    return P(unidentified)(ctx);
}
static INLINE void P(GPRnopc_Rt_addrmode_imm12_addr_S_1_STRBi12)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rs(addr, 13, 4), r(Rt));
}
static INLINE void P(GPRnopc_Rt_ldst_so_reg_shift_S_1_STRBrs)(tdis_ctx ctx, struct bitslice shift, struct bitslice Rt) {
    data(rs(shift, 13, 4), rs(shift, 0, 4), r(Rt));
}
static INLINE void P(addr_offset_none_addr_4_LDC2L_OPTION)(tdis_ctx ctx, struct bitslice addr) {
    data(r(addr));
}
static INLINE void P(addr_offset_none_addr_S_4_STC2L_OPTION)(tdis_ctx ctx, struct bitslice addr) {
    data(r(addr));
}
static INLINE void P(addr_offset_none_addr_am2offset_imm_offset_unk_Rt_4_LDRBT_POST_IMM)(tdis_ctx ctx, UNUSED struct bitslice offset, struct bitslice Rt, struct bitslice addr) {
    data(rout(Rt), r(addr));
}
static INLINE void P(addr_offset_none_addr_am2offset_reg_offset_unk_Rt_4_LDRBT_POST_REG)(tdis_ctx ctx, struct bitslice offset, struct bitslice Rt, struct bitslice addr) {
    data(rout(Rt), r(addr), rs(offset, 0, 4));
}
static INLINE void P(addr_offset_none_addr_am3offset_offset_unk_Rt_4_LDRD_POST)(tdis_ctx ctx, struct bitslice offset, struct bitslice Rt, struct bitslice addr) {
    data(rout(Rt), r(addr), rs(offset, 0, 4));
}
static INLINE void P(addr_offset_none_addr_postidx_imm8_offset_unk_Rt_3_LDRHTi)(tdis_ctx ctx, UNUSED struct bitslice offset, struct bitslice Rt, struct bitslice addr) {
    data(rout(Rt), r(addr));
}
static INLINE void P(addr_offset_none_addr_postidx_imm8s4_offset_4_LDC2L_POST)(tdis_ctx ctx, UNUSED struct bitslice offset, struct bitslice addr) {
    data(r(addr));
}
static INLINE void P(addr_offset_none_addr_postidx_imm8s4_offset_S_4_STC2L_POST)(tdis_ctx ctx, UNUSED struct bitslice offset, struct bitslice addr) {
    data(r(addr));
}
static INLINE void P(addr_offset_none_addr_unk_Rt_13_LDA)(tdis_ctx ctx, struct bitslice Rt, struct bitslice addr) {
    data(rout(Rt), r(addr));
}
static INLINE void P(addrmode3_addr_unk_Rt_4_LDRD)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    /* ignoring Rt2 = Rt + 1, but LDRD itself isn't supposed to load PC anyway */
    unsigned addr_val = bs_get(addr, ctx->base.op);
    if (addr_val & 1 << 13)
        data_flags(DFLAG_IS_LDRD_STRD, rout(Rt), rs(addr, 9, 4));
    else
        data_flags(DFLAG_IS_LDRD_STRD, rout(Rt), rs(addr, 9, 4), rs(addr, 0, 4));
}
static INLINE void P(addrmode3_pre_addr_unk_Rt_4_LDRD_PRE)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    return P(addrmode3_addr_unk_Rt_4_LDRD)(ctx, addr, Rt);
}
static INLINE void P(addrmode5_addr_8_LDC2L_OFFSET)(tdis_ctx ctx, struct bitslice addr) {
    data(rs(addr, 9, 4));
}
static INLINE void P(addrmode5_addr_S_4_STC2L_OFFSET)(tdis_ctx ctx, struct bitslice addr) {
    data(rs(addr, 9, 4));
}
static INLINE void P(addrmode5_pre_addr_4_LDC2L_PRE)(tdis_ctx ctx, struct bitslice addr) {
    data(rs(addr, 9, 4));
}
static INLINE void P(addrmode5_pre_addr_S_4_STC2L_PRE)(tdis_ctx ctx, struct bitslice addr) {
    data(rs(addr, 9, 4));
}
static INLINE void P(addrmode_imm12_addr_unk_Rt_2_LDRBi12)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rout(Rt), rs(addr, 13, 4));
}
static INLINE void P(addrmode_imm12_pre_addr_unk_Rt_2_LDRB_PRE_IMM)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rout(Rt), rs(addr, 13, 4));
}
static INLINE void P(adrlabel_label_unk_Rd_1_ADR)(tdis_ctx ctx, struct bitslice label, struct bitslice Rd) {
    return P(pcrel)(ctx, ctx->base.pc + 8 + bs_get(label, ctx->base.op),
                    (struct arch_pcrel_info) {bs_get(Rd, ctx->base.op), PLM_ADR});
}
static INLINE void P(br_target_target_pred_p_B_1_Bcc)(tdis_ctx ctx, struct bitslice target, struct bitslice p) {
    unsigned p_val = bs_get(p, ctx->base.op);
    return P(branch)(ctx, ctx->base.pc + 8 + 4 * sext(bs_get(target, ctx->base.op), 24),
                     p_val == 0xe ? 0 : (CC_ARMCC | p_val));
}
static INLINE void P(ldst_so_reg_addr_unk_Rt_2_LDRB_PRE_REG)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rout(Rt), rs(addr, 0, 4), rs(addr, 13, 4));
}
static INLINE void P(ldst_so_reg_shift_unk_Rt_2_LDRBrs)(tdis_ctx ctx, struct bitslice shift, struct bitslice Rt) {
    data(rout(Rt), rs(shift, 0, 4), rs(shift, 13, 4));
}
static INLINE void P(tcGPR_Rm_unk_Rd_1_MOVr_TC)(tdis_ctx ctx, struct bitslice Rm, struct bitslice Rd) {
    data(rout(Rd), r(Rm));
}
static INLINE void P(unk_Rd_5_MOVTi16)(tdis_ctx ctx, struct bitslice Rd) {
    data(rout(Rd));
}
static INLINE void P(unk_Rt_13_MRC)(tdis_ctx ctx, struct bitslice Rt) {
    data(rout(Rt));
}
static INLINE void P(GPR_Rn_reglist_regs_16_LDMDA)(tdis_ctx ctx, struct bitslice regs, UNUSED struct bitslice Rn) {
    unsigned regs_val = bs_get(regs, ctx->base.op);
    if (regs_val & (1 << 15))
        return P(ret)(ctx);
    return P(unidentified)(ctx);
}
static INLINE void P(GPR_Rn_reglist_regs_S_16_STMDA)(tdis_ctx ctx, UNUSED struct bitslice regs, UNUSED struct bitslice Rn) {
    unsigned regs_val = bs_get(regs, ctx->base.op);
    if (regs_val & (1 << 15))
        return P(bad)(ctx);
    return P(unidentified)(ctx);
}
static INLINE void P(GPR_Rt_addr_offset_none_addr_unk_Rd_S_6_STLEX)(tdis_ctx ctx, struct bitslice Rt, struct bitslice Rd, struct bitslice addr) {
    data(r(addr), r(Rt), r(Rd));
}
static INLINE void P(addr_offset_none_addr_postidx_reg_Rm_unk_Rt_3_LDRHTr)(tdis_ctx ctx, struct bitslice Rm, struct bitslice Rt, struct bitslice addr) {
    data(rout(Rt), r(addr), r(Rm));
}
static INLINE void P(GPR_Rt_addr_offset_none_addr_postidx_reg_Rm_S_1_STRHTr)(tdis_ctx ctx, struct bitslice Rm, struct bitslice Rt, struct bitslice addr) {
    data(r(addr), r(Rt), r(Rm));
}
static INLINE void P(GPR_dst_B_2_BX)(tdis_ctx ctx, UNUSED struct bitslice dst) {
    return P(ret)(ctx);
}
static INLINE void P(GPR_func_3_BLX)(tdis_ctx ctx, UNUSED struct bitslice func) {
    return P(indirect_call)(ctx);
}
static INLINE void P(bl_target_func_2_BL)(tdis_ctx ctx, struct bitslice func) {
    unsigned p_val = ctx->base.op >> 28; // XXX fix this to actually be an op
    return P(branch)(ctx, ctx->base.pc + 8 + 4 * sext(bs_get(func, ctx->base.op), 24),
                     CC_CALL | (p_val == 0xe ? 0 : (CC_ARMCC | p_val)));
}

static INLINE void P(dis_arm)(tdis_ctx ctx) {
    uint32_t op = ctx->base.op = unaligned_r32(ctx->base.ptr);
    ctx->base.op_size = ctx->base.newop_size = 4;
    #include "../generated/generic-dis-arm.inc.h"
    __builtin_abort();
}
#define GENERATED_HEADER "../generated/generic-dis-arm.inc.h"
