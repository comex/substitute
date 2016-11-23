#pragma once
#define MIN_INSN_SIZE 2
/* each input instruction might turn into:
 * - 2 bytes for Bcc, if in IT
 * then ONE of:
 * - 2/4 bytes for just the instruction
 * - 2+8 bytes for branch (which in *valid* code rules out IT but whatever)
 * - up to 7 4-byte insns for pcrel (if dest=pc, and while these can be subject
 *   to IT, there can only reasonably be two per block, and if there are both
 *   then that's an unconditional exit - but we don't enforce any of this
 *   currently)
 * - up to 7 4-byte insns for similar moves to PC that fall under 'data'
 * the maximum number of possible inputs is 4, plus 4 extras if the last one
 * was an IT (but in that case it can't be one of the above cases)
 * while this looks huge, it's overly conservative and doesn't matter much,
 * since only the actually used space will be taken up in the final output
 */
#define TD_MAX_REWRITTEN_SIZE (7*4*7 + 4) /* 196 */
#define ARCH_MAX_CODE_ALIGNMENT 4

struct arch_pcrel_info {
    unsigned reg;
    enum pcrel_load_mode load_mode;
};

struct arch_dis_ctx {
    /* thumb? */
    bool pc_low_bit;
    /* if thumb, IT cond for the next 5 instructions
     * (5 because we still advance after IT) */
    uint8_t it_conds[5];
    /* for transform_dis - did we add space for a Bccrel? */
    uint8_t bccrel_bits;
    void *bccrel_p;
};

static inline void arch_dis_ctx_init(struct arch_dis_ctx *ctx) {
    ctx->pc_low_bit = false;
    ctx->bccrel_p = NULL;
    memset(ctx->it_conds, 0xe, 5);
}

static inline int arch_code_alignment(struct arch_dis_ctx ctx) {
   return ctx.pc_low_bit ? 2 : 4;
}

static inline void advance_it_cond(struct arch_dis_ctx *ctx) {
    ctx->it_conds[0] = ctx->it_conds[1];
    ctx->it_conds[1] = ctx->it_conds[2];
    ctx->it_conds[2] = ctx->it_conds[3];
    ctx->it_conds[3] = ctx->it_conds[4];
    ctx->it_conds[4] = 0xe;
}

#define DFLAG_IS_LDRD_STRD (1 << 16)

/* Types of conditionals for 'branch' */
/* a regular old branch-with-condition */
#define CC_ARMCC         (CC_CONDITIONAL | 0x400)
/* already in an IT block - in transform_dis this will be rewritten to a branch
 * anyway, so it can be treated as unconditional; in jump_dis we have to know
 * to keep going */
#define CC_ALREADY_IN_IT (CC_CONDITIONAL | 0x800)
/* CBZ/CBNZ is rewritten */
#define CC_CBXZ          (CC_CONDITIONAL | 0x1000)
