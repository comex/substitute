#include "arm64/assemble.h"

static NOINLINE UNUSED
void transform_dis_pcrel(struct transform_dis_ctx *ctx, uint_tptr dpc,
                         struct arch_pcrel_info info) {
    ctx->write_newop_here = NULL;
    void **codep = ctx->rewritten_ptr_ptr;
    if (info.load_mode >= PLM_U32_SIMD) {
        int temp = arm64_get_unwritten_temp_reg(&ctx->arch);
        MOVi64(codep, 0, dpc);
        LDRxi(codep, temp, 0, 0, true, info.load_mode);
    } else {
        MOVi64(codep, info.reg, dpc);
        LDRxi(codep, info.reg, info.reg, 0, true, info.load_mode);
    }
}

static NOINLINE UNUSED
void transform_dis_branch(struct transform_dis_ctx *ctx, uint_tptr dpc, int cc) {
    /* TODO fix BL */
#ifdef TRANSFORM_DIS_VERBOSE
    printf("transform_dis (0x%llx): branch => 0x%llx\n",
           (unsigned long long) ctx->base.pc,
           (unsigned long long) dpc);
#endif
    transform_dis_branch_top(ctx, dpc, cc);
    ctx->write_newop_here = NULL;
    int mov_br_size = size_of_MOVi64(dpc) + 4;

    void **codep = ctx->rewritten_ptr_ptr;
    if ((cc & CC_ARMCC) == CC_ARMCC) {
        int icc = (cc & 0xf) ^ 1;
        Bccrel(codep, icc, 4 + mov_br_size);
    } else if ((cc & CC_XBXZ) == CC_XBXZ) {
        ctx->base.modify = true;
        ctx->base.newval[0] = ctx->base.pc + 4 + mov_br_size;
        ctx->base.newval[1] = 1; /* do invert */
        ctx->write_newop_here = *codep; *codep += 4;
    }
    int reg = arm64_get_unwritten_temp_reg(&ctx->arch);
    MOVi64(codep, reg, dpc);
    BR(codep, reg, /*link*/ cc & CC_CALL);
}

static void transform_dis_pre_dis(UNUSED struct transform_dis_ctx *ctx) {}
static void transform_dis_post_dis(struct transform_dis_ctx *ctx) {
    uint32_t op = ctx->base.op;
    ctx->arch.regs_possibly_written |= 1 << (op & 31);
    ctx->arch.regs_possibly_written |= 1 << (op >> 10 & 31);
    ctx->arch.regs_possibly_written |= 1 << (op >> 16 & 31);
}
