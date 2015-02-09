#pragma once
#define MIN_INSN_SIZE 1
#define TD_MAX_REWRITTEN_SIZE 100 /* XXX */

struct arch_pcrel_info {
    int reg;
};

struct arch_dis_ctx {};
static inline void arch_dis_ctx_init(UNUSED struct arch_dis_ctx *ctx) {}
