#include "dis.h"

static inline unsigned flip16(unsigned op) {
    return op >> 16 | op << 16;
}

static inline enum pcrel_load_mode get_thumb2_load_mode(unsigned op) {
    op = flip16(op);
    bool sign = (op >> 8) & 1;
    switch ((op >> 5) & 3) {
        case 0: return sign ? PLM_S8 : PLM_U8;
        case 1: return sign ? PLM_S16 : PLM_U16;
        case 2: return PLM_U32;
        default: __builtin_abort();
    }
}

static INLINE void P(GPR_Rm_unk_Rd_1_t2MOVr)(tdis_ctx ctx, struct bitslice Rm, struct bitslice Rd) {
    data(rout(Rd), r(Rm));
}
static INLINE void P(GPR_Rn_reglist_regs_4_t2LDMDB)(tdis_ctx ctx, struct bitslice regs, UNUSED struct bitslice Rn) {
    unsigned regs_val = bs_get(regs, ctx->base.op);
    if(regs_val & (1 << 15))
        return P(ret)(ctx);
    return P(unidentified)(ctx);
}
static INLINE void P(GPR_Rn_reglist_regs_S_4_t2STMDB)(tdis_ctx ctx, UNUSED struct bitslice regs, UNUSED struct bitslice Rn) {
    return P(unidentified)(ctx);
}
static INLINE void P(GPR_Rn_unk_Rd_1_t2ADDri12)(tdis_ctx ctx, struct bitslice Rd, struct bitslice Rn) {
    data(rout(Rd), r(Rn));
}
static INLINE void P(GPR_Rt_8_VMOVDRR)(tdis_ctx ctx, UNUSED struct bitslice Rt) {
    return P(unidentified)(ctx); /* don't care */
}
static INLINE void P(GPR_Rt_t2addrmode_imm12_addr_S_1_t2STRi12)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rs(addr, 13, 4), r(Rt));
}
static INLINE void P(GPR_Rt_t2addrmode_negimm8_addr_S_1_t2STRi8)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rs(addr, 9, 4), r(Rt));
}
static INLINE void P(GPR_Rt_t2addrmode_so_reg_addr_S_1_t2STRs)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rs(addr, 6, 4), rs(addr, 2, 4), r(Rt));
}
static INLINE void P(GPRnopc_Rn_rGPR_Rm_unk_Rd_1_t2ADDrr)(tdis_ctx ctx, struct bitslice Rm, struct bitslice Rd, struct bitslice Rn) {
    data(rout(Rd), r(Rm), r(Rn));
}
static INLINE void P(GPRnopc_Rn_unk_Rd_2_t2ADDri)(tdis_ctx ctx, struct bitslice Rd, struct bitslice Rn) {
    data(rout(Rd), r(Rn));
}
static INLINE void P(GPRnopc_Rt_t2addrmode_imm8_pre_addr_S_1_t2STR_PRE)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rs(addr, 9, 4), r(Rt));
}
static INLINE void P(GPRnopc_Rt_addr_offset_none_Rn_t2am_imm8_offset_offset_S_1_t2STR_POST)(tdis_ctx ctx, UNUSED struct bitslice offset, struct bitslice Rt, struct bitslice Rn) {
    data(r(Rt), r(Rn));
}
static INLINE void P(rGPR_Rt_addr_offset_none_addr_S_4_t2STL)(tdis_ctx ctx, struct bitslice Rt, struct bitslice addr) {
    data(rout(Rt), r(addr));
}
static INLINE void P(rGPR_Rt_addr_offset_none_addr_unk_Rd_S_7_t2STLEX)(tdis_ctx ctx, struct bitslice Rd, struct bitslice Rt, struct bitslice addr) {
    data(rout(Rd), r(Rt), r(addr));
}
static INLINE void P(addr_offset_none_addr_4_t2LDC2L_OPTION)(tdis_ctx ctx, struct bitslice addr) {
    data(r(addr));
}
static INLINE void P(addr_offset_none_addr_S_4_t2STC2L_OPTION)(tdis_ctx ctx, struct bitslice addr) {
    data(r(addr));
}
static INLINE void P(addr_offset_none_addr_postidx_imm8s4_offset_4_t2LDC2L_POST)(tdis_ctx ctx, UNUSED struct bitslice offset, struct bitslice addr) {
    data(r(addr));
}
static INLINE void P(addr_offset_none_addr_postidx_imm8s4_offset_S_4_t2STC2L_POST)(tdis_ctx ctx, UNUSED struct bitslice offset, struct bitslice addr) {
    data(r(addr));
}
static INLINE void P(addr_offset_none_addr_unk_Rt_11_t2LDA)(tdis_ctx ctx, struct bitslice Rt, struct bitslice addr) {
    data(rout(Rt), r(addr));
}
static INLINE void P(addrmode5_addr_8_VLDRD)(tdis_ctx ctx, struct bitslice addr) {
    data(rs(addr, 9, 4));
}
static INLINE void P(addrmode5_addr_S_4_t2STC2L_OFFSET)(tdis_ctx ctx, struct bitslice addr) {
    data(rs(addr, 9, 4));
}
static INLINE void P(addrmode5_pre_addr_4_t2LDC2L_PRE)(tdis_ctx ctx, struct bitslice addr) {
    data(rs(addr, 9, 4));
}
static INLINE void P(addrmode5_pre_addr_S_4_t2STC2L_PRE)(tdis_ctx ctx, struct bitslice addr) {
    data(rs(addr, 9, 4));
}
static INLINE void P(brtarget_target_pred_p_B_1_t2Bcc)(tdis_ctx ctx, struct bitslice target, struct bitslice p) {
    return P(branch)(ctx, ctx->base.pc + 4 + 2 * sext(bs_get(target, ctx->base.op), 20), 
                     CC_ARMCC | bs_get(p, ctx->base.op));
}
static INLINE void P(rGPR_Rt_t2addrmode_imm0_1020s4_addr_unk_Rd_S_1_t2STREX)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt, struct bitslice Rd) {
    data(rout(Rd), r(Rt), rs(addr, 8, 4));
}
static INLINE void P(rGPR_Rt_t2addrmode_imm12_addr_S_2_t2STRBi12)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(r(Rt), rs(addr, 13, 4));
}
static INLINE void P(rGPR_Rt_t2addrmode_imm8_pre_addr_S_2_t2STRB_PRE)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(r(Rt), rs(addr, 9, 4));
}
static INLINE void P(rGPR_Rt_t2addrmode_imm8s4_addr_S_1_t2STRDi8)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data_flags(DFLAG_IS_LDRD_STRD, r(Rt), rs(addr, 9, 4));
}
static INLINE void P(rGPR_Rt_t2addrmode_imm8s4_pre_addr_S_1_t2STRD_PRE)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data_flags(DFLAG_IS_LDRD_STRD, r(Rt), rs(addr, 9, 4));
}
static INLINE void P(rGPR_Rt_t2addrmode_negimm8_addr_S_2_t2STRBi8)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(r(Rt), rs(addr, 9, 4));
}
static INLINE void P(rGPR_Rt_t2addrmode_so_reg_addr_S_2_t2STRBs)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rs(addr, 6, 4), rs(Rt, 2, 4), r(Rt));
}
static INLINE void P(rGPR_Rt_addr_offset_none_Rn_t2am_imm8_offset_offset_S_2_t2STRB_POST)(tdis_ctx ctx, UNUSED struct bitslice offset, struct bitslice Rt, struct bitslice Rn) {
    data(r(Rt), r(Rn));
}
static INLINE void P(t2addrmode_imm0_1020s4_addr_unk_Rt_1_t2LDREX)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rout(Rt), rs(addr, 8, 4));
}
static INLINE void P(t2addrmode_imm12_addr_unk_Rt_5_t2LDRBi12)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rout(Rt), rs(addr, 13, 4));
}
static INLINE void P(t2addrmode_imm8_addr_unk_Rt_S_3_t2STRBT)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(r(Rt), rs(addr, 9, 4));
}
static INLINE void P(t2addrmode_imm8_pre_addr_unk_Rt_5_t2LDRB_PRE)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rout(Rt), rs(addr, 9, 4));
}
static INLINE void P(addr_offset_none_Rn_t2am_imm8_offset_offset_unk_Rt_5_t2LDRB_POST)(tdis_ctx ctx, UNUSED struct bitslice offset, struct bitslice Rt, struct bitslice Rn) {
    data(rout(Rt), r(Rn));
}
static INLINE void P(t2addrmode_imm8s4_addr_unk_Rt_1_t2LDRDi8)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data_flags(DFLAG_IS_LDRD_STRD, rout(Rt), rs(addr, 9, 4));
}
static INLINE void P(t2addrmode_imm8s4_pre_addr_unk_Rt_1_t2LDRD_PRE)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data_flags(DFLAG_IS_LDRD_STRD, rout(Rt), rs(addr, 9, 4));
}
static INLINE void P(t2addrmode_negimm8_addr_unk_Rt_5_t2LDRBi8)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rout(Rt), rs(addr, 9, 4));
}
static INLINE void P(t2addrmode_posimm8_addr_unk_Rt_5_t2LDRBT)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rout(Rt), rs(addr, 9, 4));
}
static INLINE void P(t2addrmode_so_reg_addr_unk_Rt_5_t2LDRBs)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    data(rout(Rt), rs(addr, 6, 4), rs(addr, 2, 4));
}
static INLINE void P(t2adrlabel_addr_unk_Rd_1_t2ADR)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rd) {
    return P(pcrel)(ctx, ((ctx->base.pc + 4) & ~2) +
                         (bs_get(addr, ctx->base.op) & ((1 << 12) - 1)),
                    (struct arch_pcrel_info) {bs_get(Rd, ctx->base.op), PLM_ADR});
}
static INLINE void P(t2ldrlabel_addr_unk_Rt_5_t2LDRBpci)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    return P(pcrel)(ctx, ((ctx->base.pc + 4) & ~2) +
                         (bs_get(addr, ctx->base.op) & ((1 << 12) - 1)),
                    (struct arch_pcrel_info) {bs_get(Rt, ctx->base.op),
                                              get_thumb2_load_mode(ctx->base.op)});
}
static INLINE void P(uncondbrtarget_target_B_1_t2B)(tdis_ctx ctx, struct bitslice target) {
    int cc = ctx->arch.it_conds[0] != 0xe ? CC_ALREADY_IN_IT : 0;
    return P(branch)(ctx, ctx->base.pc + 4 + 2 * sext(bs_get(target, ctx->base.op), 24), cc);
}
static INLINE void P(unk_Rd_3_t2MOVTi16)(tdis_ctx ctx, struct bitslice Rd) {
    data(rout(Rd));
}
static INLINE void P(unk_Rm_B_2_t2TBB)(tdis_ctx ctx, UNUSED struct bitslice Rm) {
    /* Ew.  Don't bother for now - this is hard to make show up in the first 8 bytes... */
    return P(bad)(ctx);
}
static INLINE void P(unk_Rt_13_VMOVRRD)(tdis_ctx ctx, UNUSED struct bitslice Rt) {
    return P(unidentified)(ctx);
}
static INLINE void P(t_bltarget_func_1_tBL)(tdis_ctx ctx, struct bitslice func) {
    unsigned crap = bs_get(func, ctx->base.op) << 1; // first bit zero
    unsigned S = crap >> 24 & 1;
    if (!S)
        crap ^= (3 << 22);
    return P(branch)(ctx, ctx->base.pc + 4 + sext(crap, 25), CC_CALL);

}
static INLINE void P(t_blxtarget_func_1_tBLXi)(tdis_ctx ctx, struct bitslice func) {
    unsigned crap = bs_get(func, ctx->base.op) << 1; // first two bits zero
    unsigned S = crap >> 24 & 1;
    if (!S)
        crap ^= (3 << 22);
    return P(branch)(ctx, ctx->base.pc + 4 + sext(crap, 25), CC_CALL);
}
static INLINE void P(rGPR_func_1_t2BXJ)(tdis_ctx ctx, UNUSED struct bitslice func) {
    return P(unidentified)(ctx);
}

static INLINE void P(thumb2_do_it)(tdis_ctx ctx) {
    uint32_t op = ctx->base.op;
    #include "../generated/generic-dis-thumb2.inc.h"
    __builtin_abort();
}

static INLINE void P(dis_thumb2)(tdis_ctx ctx) {
    ctx->base.op = unaligned_r32(ctx->base.ptr);
    ctx->base.op_size = ctx->base.newop_size = 4;
    /* LLVM likes to think about Thumb2 instructions the way the ARM manual
     * does - 15..0 15..0 rather than 31..0 as actually laid out in memory... */
    ctx->base.op = flip16(ctx->base.op);
    P(thumb2_do_it)(ctx);
    advance_it_cond(&ctx->arch);
    uint32_t *newop_p = (uint32_t *) ctx->base.newop;
    *newop_p = flip16(*newop_p);
    ctx->base.op = flip16(ctx->base.op);
}
