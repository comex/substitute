#pragma once
#define MIN_INSN_SIZE 4
#define TD_MAX_REWRITTEN_SIZE (7 * 2 * 4) /* also conservative */
#define ARCH_MAX_CODE_ALIGNMENT 4

struct arch_pcrel_info {
    unsigned reg;
    enum pcrel_load_mode load_mode;
};

struct arch_dis_ctx {
    /* For transform_dis only - used to get temporary registers.  We assume
     * that we can use any caller-saved or IP register which was not written,
     * so r9-r18.
     * This is a massive overestimate: we just OR in each instruction's bits
     * 4:0 (Rd for data, Rt for loads, most common), 14:10 (Rt2 for load-pair
     * instructions), and 20:16 (Rs for store-exclusive insturctions).  It
     * would be easy to restrict the latter two to the few instructions that
     * actually use them, but with 10 available registers, and a patch of at
     * most 3 instructions (and none of the instructions that require a temp
     * use Rt2/Rs or could read their Rd, so the third doesn't count), we won't
     * run out even with the dumbest possible thing. */
    uint32_t regs_possibly_written;
};

static inline void arch_dis_ctx_init(struct arch_dis_ctx *ctx) {
    ctx->regs_possibly_written = 0;
}

static inline int arm64_get_unwritten_temp_reg(struct arch_dis_ctx *ctx) {
    uint32_t avail = ~ctx->regs_possibly_written & ((1 << 19) - (1 << 9));
    if (!avail)
        __builtin_abort();
    return 31 - __builtin_clz(avail);
}

#define CC_ARMCC         (CC_CONDITIONAL | 0x400)
#define CC_XBXZ          (CC_CONDITIONAL | 0x800)
