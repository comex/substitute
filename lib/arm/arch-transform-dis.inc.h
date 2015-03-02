/* TODO fix BL incl MOV LR, PC */
#include "arm/assemble.h"

static struct assemble_ctx tdctx_to_actx(const struct transform_dis_ctx *ctx) {
    int cond;
    if (ctx->arch.pc_low_bit) {
        cond = ctx->base.op >> 28;
        if (cond == 0xf)
            cond = 0xe;
    } else {
        cond = 0;
    }
    return (struct assemble_ctx) {
        ctx->rewritten_ptr_ptr,
        (uint_tptr) (uintptr_t) ctx->rewritten_ptr_ptr,
        ctx->arch.pc_low_bit,
        cond
    };

}

static int invert_arm_cond(int cc) {
    if (cc >= 0xe)
        __builtin_abort();
    return cc ^ 1;
}

static NOINLINE UNUSED
void transform_dis_data(struct transform_dis_ctx *ctx, unsigned o0, unsigned o1,
                        unsigned o2, unsigned o3, unsigned out_mask) {
#ifdef TRANSFORM_DIS_VERBOSE
    printf("transform_dis_data: (0x%llx) %x %x %x %x out_mask=%x\n",
           (unsigned long long) ctx->base.pc,
           o0, o1, o2, o3, out_mask);
#endif
    /* We only care if at least one op is PC, so quickly approximate that. */
    if (((o0 | o1 | o2 | o3) & 15) != 15)
        return;
    unsigned *newval = ctx->base.newval;
    newval[0] = o0;
    newval[1] = o1;
    newval[2] = o2;
    newval[3] = o3;

    void **codep = ctx->rewritten_ptr_ptr;
    struct assemble_ctx actx = tdctx_to_actx(ctx);

    /* A few cases:
     * 1. Move to PC that does not read PC.  Probably fine.
     * 2. Move to PC that does read PC, e.g. 'ldrls pc, [pc, r0, lsl #2]'.
     *    This is different from #4 mainly in that we can't need to do
     *    something like pop {temp, pc}.  Not terribly plausible (only likely
     *    in non-position-independent code in ARM mode, and I can't get it to
     *    happen in the first 8 bytes then), but we may as well handle it.
     * 3. Read of PC that does not read the register(s) it writes, e.g. adr r3,
     *    X.  In this case we can use that register as a temporary.
     * 4. Read of PC that does, or doesn't have any output register, e.g. add
     *    r3, pc.  In this case we use the stack because reliably finding a
     *    free register would be work, and might not even be possible (thumb
     *    mov r9, r0; mov r12, r1; <some PC using insn(s)>)
     * the out register is always first.
     */
    uint16_t in_regs = 0;
    int out_reg = -1;
    for (int i = 0; i < 4; i++) {
        if (out_mask & 1 << i)
            out_reg = newval[i];
        else if (newval[i] != null_op)
            in_regs |= 1 << newval[i];
    }
    if (out_mask & DFLAG_IS_LDRD_STRD)
        in_regs |= 1 << (newval[0] + 1);
    uint32_t pc = ctx->base.pc + (ctx->arch.pc_low_bit ? 4 : 8);
    int scratch = __builtin_ctz(~(in_regs | (1 << out_reg)));

#ifdef TRANSFORM_DIS_VERBOSE
    printf("transform_dis_data: in_regs=%x out_reg=%d pc=%x scratch=%d\n",
           in_regs, out_reg, pc, scratch);
#endif

    if (out_reg == 15) {
        if (in_regs & 1 << 15)
            return; /* case 1 */
        /* case 2 */
        PUSHone(actx, scratch);
        PUSHone(actx, scratch);
        MOVW_MOVT(actx, scratch, pc);
        for (int i = 0; i < 4; i++)
            if (newval[i] == 15)
                newval[i] = scratch;
        ctx->write_newop_here = *codep; *codep += ctx->base.op_size;
        STRri(actx, scratch, 13, 4);
        POPmulti(actx, 1 << scratch | 1 << 15);
        if (actx.cond != 0xe)
            transform_dis_ret(ctx);
    } else {
        if (!(in_regs & 1 << 15))
            return;
        if (out_reg != -1 && !(in_regs & 1 << out_reg)) {
            /* case 3 - ignore scratch */
            MOVW_MOVT(actx, out_reg, pc);
            for (int i = 0; i < 4; i++)
                if (newval[i] == 15)
                    newval[i] = out_reg;
            ctx->write_newop_here = *codep; *codep += ctx->base.op_size;
        } else {
            /* case 4 */
            PUSHone(actx, scratch);
            MOVW_MOVT(actx, scratch, pc);
            for (int i = 0; i < 4; i++)
                if (newval[i] == 15)
                    newval[i] = scratch;
            ctx->write_newop_here = *codep; *codep += ctx->base.op_size;
            POPone(actx, scratch);
        }
    }
    ctx->base.modify = true;
#ifdef TRANSFORM_DIS_VERBOSE
    printf("transform_dis_data: => %x %x %x %x\n",
           newval[0], newval[1], newval[2], newval[3]);
#endif
}

static NOINLINE UNUSED
void transform_dis_pcrel(struct transform_dis_ctx *ctx, uint_tptr dpc,
                         struct arch_pcrel_info info) {
#ifdef TRANSFORM_DIS_VERBOSE
    printf("transform_dis_pcrel: (0x%llx) dpc=0x%llx reg=%x mode=%d\n",
           (unsigned long long) ctx->base.pc,
           (unsigned long long) dpc,
           info.reg, info.load_mode);
#endif
    ctx->write_newop_here = NULL;
    struct assemble_ctx actx = tdctx_to_actx(ctx);
    if (info.reg == 15) {
        int scratch = 0;
        PUSHone(actx, scratch);
        PUSHone(actx, scratch);
        MOVW_MOVT(actx, scratch, dpc);
        if (info.load_mode != PLM_ADR)
            LDRxi(actx, scratch, scratch, 0, info.load_mode);
        STRri(actx, scratch, 13, 4);
        POPmulti(actx, 1 << scratch | 1 << 15);
        transform_dis_ret(ctx);
    } else {
        MOVW_MOVT(actx, info.reg, dpc);
        if (info.load_mode != PLM_ADR)
            LDRxi(actx, info.reg, info.reg, 0, info.load_mode);
    }
}

static NOINLINE UNUSED
void transform_dis_branch(struct transform_dis_ctx *ctx, uint_tptr dpc, int cc) {
#ifdef TRANSFORM_DIS_VERBOSE
    printf("transform_dis (0x%llx): branch => 0x%llx\n",
           (unsigned long long) ctx->base.pc,
           (unsigned long long) dpc);
#endif
    /* The check in transform_dis_branch_top is correct under the simplifying
     * assumption here that functions will not try to branch into the middle of
     * an IT block, which is the case where pc_patch_end changes to include
     * additional instructions (as opposed to include the end of a partially
     * included instruction, which is common). */
    transform_dis_branch_top(ctx, dpc, cc);
    struct assemble_ctx actx = tdctx_to_actx(ctx);
    ctx->write_newop_here = NULL;
    if ((cc & CC_ARMCC) == CC_ARMCC) {
        actx.cond = invert_arm_cond(cc & 0xf);
        Bccrel(actx, 2+8);
    } else if ((cc & CC_CBXZ) == CC_CBXZ) {
        ctx->base.modify = true;
        ctx->base.newval[0] = ctx->base.pc + 2+8;
        ctx->base.newval[1] = 1; /* do invert */
        void **codep = ctx->rewritten_ptr_ptr;
        ctx->write_newop_here = *codep; *codep += 2;
    }
    actx.cond = 0xe;
    LDR_PC(actx, dpc | ctx->arch.pc_low_bit);
}

static void transform_dis_pre_dis(struct transform_dis_ctx *ctx) {
    /* for simplicity we turn IT into a series of branches for each
     * instruction, so... */
    if (ctx->arch.it_conds[0] != 0xe) {
        ctx->arch.bccrel_bits = invert_arm_cond(ctx->arch.it_conds[0]);
        ctx->arch.bccrel_p = *ctx->rewritten_ptr_ptr;
        *ctx->rewritten_ptr_ptr += 2;
    } else {
        ctx->arch.bccrel_p = NULL;
    }
}

static void transform_dis_post_dis(struct transform_dis_ctx *ctx) {
    if (ctx->arch.bccrel_p) {
        struct assemble_ctx actx = {&ctx->arch.bccrel_p,
                                    (uint_tptr) (uintptr_t) ctx->arch.bccrel_p,
                                    /*thumb*/ true,
                                    ctx->arch.bccrel_bits};
        Bccrel(actx, *ctx->rewritten_ptr_ptr - ctx->arch.bccrel_p);
    }
    ctx->force_keep_transforming = ctx->arch.it_conds[0] != 0xe;
}
