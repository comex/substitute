#include "dis.h"
static INLINE void P(GPR_Rm_unk_Rdn_1_tADDhirr)(tdis_ctx ctx, struct bitslice Rdn, struct bitslice Rm) {
    data(rout(Rdn), r(Rm), r(Rdn)); /* yes, twice */
}
static INLINE void P(unk_Rdn_1_tADDrSP)(tdis_ctx ctx, UNUSED struct bitslice Rdn) {
    /* this doesn't support constants, and nobody's going to add pc, sp, so... */
    return P(unidentified)(ctx);
}
static INLINE void P(GPR_Rm_1_tADDspr)(tdis_ctx ctx, UNUSED struct bitslice Rm) {
    /* ditto */
    return P(unidentified)(ctx);
}
static INLINE void P(GPR_Rm_B_1_tBX)(tdis_ctx ctx, struct bitslice Rm) {
    unsigned val = bs_get(Rm, ctx->base.op);
    if (val == 15) /* bx pc */
        return P(bad)(ctx);
    return P(ret)(ctx);
}
static INLINE void P(GPR_Rm_unk_Rd_1_tMOVr)(tdis_ctx ctx, struct bitslice Rd, struct bitslice Rm) {
    unsigned Rd_val = bs_get(Rd, ctx->base.op);
    unsigned Rm_val = bs_get(Rm, ctx->base.op);
    /* does anyone do this? */
    if (Rd_val == 15)
        return P(bad)(ctx);
    if (Rm_val == 15)
        return P(pcrel)(ctx, ctx->base.pc + 4,
                        (struct arch_pcrel_info) {Rd_val, PLM_ADR});
    return P(unidentified)(ctx);
}
static INLINE void P(tGPR_Rn_reglist_regs_1_tLDMIA)(tdis_ctx ctx, UNUSED struct bitslice regs, UNUSED struct bitslice Rn) {
    return P(unidentified)(ctx);
}
static INLINE void P(tGPR_Rn_reglist_regs_S_1_tSTMIA_UPD)(tdis_ctx ctx, UNUSED struct bitslice regs, UNUSED struct bitslice Rn) {
    return P(unidentified)(ctx);
}
static INLINE void P(reglist_regs_1_tPOP)(tdis_ctx ctx, struct bitslice regs) {
    unsigned regs_val = bs_get(regs, ctx->base.op);
    if(regs_val & (1 << 15))
        return P(ret)(ctx);
    return P(unidentified)(ctx);
}
static INLINE void P(reglist_regs_S_1_tPUSH)(tdis_ctx ctx, UNUSED struct bitslice regs) {
    return P(unidentified)(ctx);
}
static INLINE void P(t_addrmode_pc_addr_unk_Rt_1_tLDRpci)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    return P(pcrel)(ctx, ((ctx->base.pc + 4) & ~2) + bs_get(addr, ctx->base.op),
                    (struct arch_pcrel_info) {bs_get(Rt, ctx->base.op), PLM_U32});
}
static INLINE void P(t_adrlabel_addr_unk_Rd_1_tADR)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rd) {
    return P(pcrel)(ctx, ((ctx->base.pc + 4) & ~2) + bs_get(addr, ctx->base.op),
                    (struct arch_pcrel_info) {bs_get(Rd, ctx->base.op), PLM_ADR});
}
static INLINE void P(t_bcctarget_target_pred_p_B_1_tBcc)(tdis_ctx ctx, struct bitslice target, struct bitslice p) {
    return P(branch)(ctx, ctx->base.pc + 4 + 2 * sext(bs_get(target, ctx->base.op), 8),
                     CC_ARMCC | bs_get(p, ctx->base.op));
}
static INLINE void P(t_brtarget_target_B_1_tB)(tdis_ctx ctx, struct bitslice target) {
    int cc = ctx->arch.it_conds[0] != 0xe ? CC_ALREADY_IN_IT : 0;
    return P(branch)(ctx, ctx->base.pc + 4 + 2 * sext(bs_get(target, ctx->base.op), 11), cc);
}
static INLINE void P(t_cbtarget_target_B_2_tCBNZ)(tdis_ctx ctx, struct bitslice target) {
    P(branch)(ctx, ctx->base.pc + 4 + 2 * bs_get(target, ctx->base.op), CC_CBXZ);
    if (ctx->base.modify) {
        /* change target, and flip z/nz if necessary (i.e. always) */
        int new_target = (ctx->base.newop[0] - (ctx->base.pc + 4)) / 2;
        unsigned new = bs_set(target, new_target, ctx->base.op);
        if (ctx->base.newop[1])
            new ^= 1 << 11;
        *(uint32_t *) ctx->base.newop = new;
    }
}
static INLINE void P(it_pred_cc_it_mask_mask_1_t2IT)(tdis_ctx ctx, struct bitslice mask, struct bitslice cc) {
    /* why */
    unsigned mask_val = bs_get(mask, ctx->base.op);
    unsigned cc_val = bs_get(cc, ctx->base.op);
    if (mask_val == 0)
        return P(unidentified)(ctx); /* nop */
    int length = 4 - __builtin_ctz(mask_val);
    ctx->arch.it_conds[1] = cc_val;
    for (int i = 0; i < length; i++)
        ctx->arch.it_conds[i+2] = (cc_val & ~1) | (mask_val >> (3 - i) & 1);
    return P(thumb_it)(ctx);
}
static INLINE void P(GPR_func_1_tBLXr)(tdis_ctx ctx, UNUSED struct bitslice func) {
    return P(indirect_call)(ctx);
}

static INLINE void P(thumb_do_it)(tdis_ctx ctx) {
    uint16_t op = ctx->base.op = unaligned_r16(ctx->base.ptr);
    #include "../generated/generic-dis-thumb.inc.h"
    __builtin_abort();
}

static INLINE void P(dis_thumb)(tdis_ctx ctx) {
    ctx->base.op_size = ctx->base.newop_size = 2;
    P(thumb_do_it)(ctx);
    advance_it_cond(&ctx->arch);
}
