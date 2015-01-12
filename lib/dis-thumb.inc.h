#include "dis.h"
static INLINE tdis_ret P(GPR_Rm_2_tADDhirr)(tdis_ctx ctx, struct bitslice Rm) {
    return P(regs)(ctx, Rm, 0, 3, Rm, 0, 0);
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
        return P(pcrel)(ctx, ctx->pc + 4, Rd_val);
    return P(unidentified)(ctx);
}
static INLINE tdis_ret P(t_addrmode_pc_addr_1_tLDRpci)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rt) {
    return P(pcrel)(ctx, ((ctx->pc + 4) & ~2) + bs_get(addr, ctx->op), bs_get(Rt, ctx->op), true);
}
static INLINE tdis_ret P(t_adrlabel_addr_1_tADR)(tdis_ctx ctx, struct bitslice addr, struct bitslice Rd) {
    return P(pcrel)(ctx, ((ctx->pc + 4) & ~2) + bs_get(addr, ctx->op), bs_get(Rd, ctx->op), false);
}
static INLINE tdis_ret P(t_bcctarget_target_B_1_tBcc)(tdis_ctx ctx, struct bitslice target) {
    return P(branch)(ctx->pc + 4 + 2 * sext(bs_get(target, ctx->op), 8);
}
static INLINE tdis_ret P(t_brtarget_target_B_1_tB)(tdis_ctx ctx, struct bitslice target) {
    return P(branch)(ctx->pc + 4 + 2 * sext(bs_get(target, ctx->op), 11);
}
static INLINE tdis_ret P(t_cbtarget_target_B_2_tCBNZ)(tdis_ctx ctx, struct bitslice target) {
    return P(branch)(ctx->pc + 4 + 2 * bs_get(target, ctx->op);
}

static tdis_ret P(dis_thumb)(tdis_ctx ctx) {
    unsigned op = ctx->op;
    #include "../generated/transform-dis-thumb.inc"
}

