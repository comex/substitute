#include "arm/assemble.h"

static NOINLINE UNUSED void transform_dis_data(struct transform_dis_ctx *ctx,
        unsigned o0, unsigned o1, unsigned o2, unsigned o3, unsigned out_mask) {
#ifdef TRANSFORM_DIS_VERBOSE
    printf("transform_dis_data: (%p) %x %x %x %x out_mask=%x\n", (void *) ctx->pc,
           o0, o1, o2, o3, out_mask);
#endif
    /* We only care if at least one op is PC, so quickly test that. */
    if (((o0 | o1 | o2 | o3) & 15) != 15)
        return;
    unsigned *newval = ctx->newval;
    newval[0] = o0;
    newval[1] = o1;
    newval[2] = o2;
    newval[3] = o3;

    void **codep = ctx->rewritten_ptr_ptr;

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
    if (out_mask & IS_LDRD_STRD)
        in_regs |= 1 << (newval[0] + 1);
    uint32_t pc = ctx->pc + (ctx->arch.pc_low_bit ? 4 : 8);
    int scratch = __builtin_ctz(~(in_regs | (1 << out_reg)));

#ifdef TRANSFORM_DIS_VERBOSE
    printf("transform_dis_data: in_regs=%x out_reg=%d pc=%x scratch=%d\n",
           in_regs, out_reg, pc, scratch);
#endif

    if (out_reg == 15) {
        if (in_regs & 1 << 15)
            return; /* case 1 */
        /* case 2 */
        PUSHone(codep, scratch);
        PUSHone(codep, scratch);
        MOVW_MOVT(codep, scratch, pc);
        for (int i = 0; i < 4; i++)
            if (newval[i] == 15)
                newval[i] = scratch;
        ctx->write_newop_here = *codep; *codep += ctx->op_size;
        STRri(codep, scratch, 13, 4);
        POPmulti(codep, 1 << scratch | 1 << 15);
        transform_dis_ret(ctx);
    } else {
        if (out_reg != -1 && !(in_regs & 1 << out_reg)) {
            /* case 3 - ignore scratch */
            MOVW_MOVT(codep, out_reg, pc);
            for (int i = 0; i < 4; i++)
                if (newval[i] == 15)
                    newval[i] = out_reg;
            ctx->write_newop_here = *codep; *codep += ctx->op_size;
        } else {
            /* case 4 */
            PUSHone(codep, scratch);
            MOVW_MOVT(codep, scratch, pc);
            for (int i = 0; i < 4; i++)
                if (newval[i] == 15)
                    newval[i] = scratch;
            ctx->write_newop_here = *rpp; *rpp += ctx->op_size;
            POPone(codep, scratch);
        }
    }
    ctx->modify = true;
#ifdef TRANSFORM_DIS_VERBOSE
    printf("transform_dis_data: => %x %x %x %x\n",
           newval[0], newval[1], newval[2], newval[3]);
#endif
}

static NOINLINE UNUSED void transform_dis_pcrel(struct transform_dis_ctx *ctx,
        uintptr_t dpc, unsigned reg, enum pcrel_load_mode load_mode) {
#ifdef TRANSFORM_DIS_VERBOSE
    printf("transform_dis_pcrel: (%p) dpc=%p reg=%x mode=%d\n", (void *) ctx->pc,
           (void *) dpc, reg, load_mode);
#endif
    ctx->write_newop_here = NULL;
    void **codep = ctx->rewritten_ptr_ptr;
    if (reg == 15) {
        int scratch = 0;
        PUSHone(codep, scratch);
        PUSHone(codep, scratch);
        MOVW_MOVT(codep, scratch, dpc);
        if (load_mode != PLM_ADR)
            LDRxi(codep, scratch, scratch, 0, load_mode);
        STRri(codep, scratch, 13, 4);
        POPmulti(codep, 1 << scratch | 1 << 15);
        transform_dis_ret(codep);
    } else {
        MOVW_MOVT(codep, reg, dpc);
        if (load_mode != PLM_ADR)
            LDRxi(codep, reg, reg, 0, load_mode);
    }
}
