#include "dis.h"
static INLINE tdis_ret P(GPR_Rm_unk_Rdn_1_tADDhirr)(tdis_ctx ctx, struct bitslice Rdn, struct bitslice Rm) {
    data(rout(Rdn), r(Rm), r(Rdn)); /* yes, twice */
}
static INLINE tdis_ret P(unk_Rdn_1_tADDrSP)(tdis_ctx ctx, UNUSED struct bitslice Rdn) {
    /* this doesn't support constants, and nobody's going to add pc, sp, so... */
    return P(unidentified)(ctx);
}
static INLINE tdis_ret P(GPR_Rm_1_tADDspr)(tdis_ctx ctx, UNUSED struct bitslice Rm) {
    /* ditto */
    return P(unidentified)(ctx);
}
static INLINE tdis_ret P(GPR_Rm_B_1_tBX)(tdis_ctx ctx, struct bitslice Rm) {
    unsigned val = bs_get(Rm, ctx->op);
    if (val == 15) /* bx pc */
        return P(bad)(ctx);
    else if (val == 14) /* bx lr */
        return P(ret)(ctx);
    return P(unidentified)(ctx);
}
static INLINE tdis_ret P(GPR_Rm_unk_Rd_1_tMOVr)(tdis_ctx ctx, struct bitslice Rd, struct bitslice Rm) {
    unsigned Rd_val = bs_get(Rd, ctx->op);
    unsigned Rm_val = bs_get(Rm, ctx->op);
    /* does anyone do this? */
    if (Rd_val == 15)
        return P(bad)(ctx);
    if (Rm_val == 15)
        return P(pcrel)(ctx, ctx->pc + 4, Rd_val, PLM_ADR);
    return P(unidentified)(ctx);
}
static INLINE tdis_ret P(tGPR_Rn_reglist_regs_1_tLDMIA)(tdis_ctx ctx, UNUSED struct bitslice regs, UNUSED struct bitslice Rn) {
    return P(unidentified)(ctx);
}
static INLINE tdis_ret P(tGPR_Rn_reglist_regs_S_1_tSTMIA_UPD)(tdis_ctx ctx, UNUSED struct bitslice regs, UNUSED struct bitslice Rn) {
    return P(unidentified)(ctx);
}
static INLINE tdis_ret P(t_addrmode_pc_addr_unk_Rt_1_tLDRpci)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    return P(pcrel)(ctx, ((ctx->pc + 4) & ~2) + bs_get(addr, ctx->op), bs_get(Rt, ctx->op), PLM_U32);
}
static INLINE tdis_ret P(t_adrlabel_addr_unk_Rd_1_tADR)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rd) {
    return P(pcrel)(ctx, ((ctx->pc + 4) & ~2) + bs_get(addr, ctx->op), bs_get(Rd, ctx->op), PLM_ADR);
}
static INLINE tdis_ret P(t_bcctarget_target_B_1_tBcc)(tdis_ctx ctx, struct bitslice target) {
    return P(branch)(ctx, ctx->pc + 4 + 2 * sext(bs_get(target, ctx->op), 8));
}
static INLINE tdis_ret P(t_brtarget_target_B_1_tB)(tdis_ctx ctx, struct bitslice target) {
    return P(branch)(ctx, ctx->pc + 4 + 2 * sext(bs_get(target, ctx->op), 11));
}
static INLINE tdis_ret P(t_cbtarget_target_B_2_tCBNZ)(tdis_ctx ctx, struct bitslice target) {
    return P(branch)(ctx, ctx->pc + 4 + 2 * bs_get(target, ctx->op));
}

#define GENERATED_HEADER "../generated/transform-dis-thumb.inc.h"
