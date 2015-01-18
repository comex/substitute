static inline void PUSHone(struct transform_dis_ctx *ctx, int Rt) {
    if (ctx->pc_low_bit)
        op32(ctx, 0x0d04f84d | Rt << 28);
    else
        op32(ctx, 0xe52d0004 | Rt << 12);
}

static inline void POPone(struct transform_dis_ctx *ctx, int Rt) {
    if (ctx->pc_low_bit)
        op32(ctx, 0x0b04f85d | Rt << 28);
    else
        op32(ctx, 0xe49d0004 | Rt << 12);
}

static inline void POPmulti(struct transform_dis_ctx *ctx, uint16_t mask) {
    if (ctx->pc_low_bit)
        op32(ctx, 0x0000e8bd | mask << 16);
    else
        op32(ctx, 0xe8bd0000 | mask);
}

static inline void MOVW_MOVT(struct transform_dis_ctx *ctx, int Rd, uint32_t val) {
    uint16_t hi = val >> 16, lo = (uint16_t) val;
    if (ctx->pc_low_bit) {
        op32(ctx, 0x0000f240 | Rd << 24 | lo >> 12 | (lo >> 11 & 1) << 10 |
                               (lo >> 8 & 7) << 28 | (lo & 0xff) << 16);
        op32(ctx, 0x0000f2c0 | Rd << 24 | hi >> 12 | (hi >> 11 & 1) << 10 |
                               (hi >> 8 & 7) << 28 | (hi & 0xff) << 16);

    } else {
        op32(ctx, 0xe3000000 | Rd << 12 | (lo >> 12) << 16 | (lo & 0xfff));
        op32(ctx, 0xe3400000 | Rd << 12 | (hi >> 12) << 16 | (hi & 0xfff));
    }

}

static inline void STRri(struct transform_dis_ctx *ctx, int Rt, int Rn, uint32_t off) {
    if (ctx->pc_low_bit)
        op32(ctx, 0x0000f8c0 | Rn | Rt << 28 | off << 16);
    else
        op32(ctx, 0xe4800000 | Rn << 16 | Rt << 12 | off);
}

static inline void LDRxi(struct transform_dis_ctx *ctx, int Rt, int Rn, uint32_t off,
                         enum pcrel_load_mode load_mode) {
    if (ctx->pc_low_bit) {
        int subop, sign;
        switch (load_mode) {
            case PLM_U8:  subop = 0; sign = 0; break;
            case PLM_S8:  subop = 0; sign = 1; break;
            case PLM_U16: subop = 1; sign = 0; break;
            case PLM_S16: subop = 1; sign = 1; break;
            case PLM_U32: subop = 2; sign = 0; break;
            default: __builtin_abort();
        }
        op32(ctx, 0x0000f890 | Rn | Rt << 28 | subop << 5 | sign << 8 | off << 16);
    } else {
        int is_byte, subop, not_ldrd;
        switch (load_mode) {
            case PLM_U8:   is_byte = 1; goto type1;
            case PLM_S8:   subop = 13; not_ldrd = 1; goto type2;
            case PLM_U16:  subop = 11; not_ldrd = 1; goto type2;
            case PLM_S16:  subop = 15; not_ldrd = 1; goto type2;
            case PLM_U32:  is_byte = 0; goto type1;
            case PLM_U128: subop = 13; not_ldrd = 0; goto type2;
            type1:
                op32(ctx, 0xe5900000 | Rn << 16 | Rt << 12 | off);
                break;
            type2:
                op32(ctx, 0xe1c00000 | Rn << 16 | Rt << 12 | subop << 4 |
                     (off & 0xf) | (off & 0xf0) << 4 | not_ldrd << 20);
                 break;
            default:
                __builtin_abort();
        }
    }
}

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

    void **rpp = ctx->rewritten_ptr_ptr;

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
    uint32_t pc = ctx->pc + (ctx->pc_low_bit ? 4 : 8);
    int scratch = __builtin_ctz(~(in_regs | (1 << out_reg)));

#ifdef TRANSFORM_DIS_VERBOSE
    printf("transform_dis_data: in_regs=%x out_reg=%d pc=%x scratch=%d\n",
           in_regs, out_reg, pc, scratch);
#endif

    if (out_reg == 15) {
        if (in_regs & 1 << 15)
            return; /* case 1 */
        /* case 2 */
        PUSHone(ctx, scratch);
        PUSHone(ctx, scratch);
        MOVW_MOVT(ctx, scratch, pc);
        for (int i = 0; i < 4; i++)
            if (newval[i] == 15)
                newval[i] = scratch;
        ctx->write_newop_here = *rpp; *rpp += ctx->op_size;
        STRri(ctx, scratch, 13, 4);
        POPmulti(ctx, 1 << scratch | 1 << 15);
    } else {
        if (out_reg != -1 && !(in_regs & 1 << out_reg)) {
            /* case 3 - ignore scratch */
            MOVW_MOVT(ctx, out_reg, pc);
            for (int i = 0; i < 4; i++)
                if (newval[i] == 15)
                    newval[i] = out_reg;
            ctx->write_newop_here = *rpp; *rpp += ctx->op_size;
        } else {
            /* case 4 */
            PUSHone(ctx, scratch);
            MOVW_MOVT(ctx, scratch, pc);
            for (int i = 0; i < 4; i++)
                if (newval[i] == 15)
                    newval[i] = scratch;
            ctx->write_newop_here = *rpp; *rpp += ctx->op_size;
            POPone(ctx, scratch);
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
    if (reg == 15) {
        int scratch = 0;
        PUSHone(ctx, scratch);
        PUSHone(ctx, scratch);
        MOVW_MOVT(ctx, scratch, dpc);
        if (load_mode != PLM_ADR)
            LDRxi(ctx, scratch, scratch, 0, load_mode);
        STRri(ctx, scratch, 13, 4);
        POPmulti(ctx, 1 << scratch | 1 << 15);
    } else {
        MOVW_MOVT(ctx, reg, dpc);
        if (load_mode != PLM_ADR)
            LDRxi(ctx, reg, reg, 0, load_mode);
    }
}
