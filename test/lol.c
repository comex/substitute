#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#define IF_BOTHER_WITH_MODIFY(...) __VA_ARGS__
#include "dis.h"

typedef struct tc {
    struct dis_ctx_base base;
    struct arch_dis_ctx arch;
} *tdis_ctx;
#define P(x) P_##x
#define DIS_MAY_MODIFY 0

static enum {
    NOPPY,
    JUMPY,
    BAD
} type;

NOINLINE UNUSED
static void P_data(UNUSED struct tc *ctx, unsigned o0, unsigned o1, unsigned o2,
                   unsigned o3, unsigned out_mask) {
    unsigned ops[] = {o0, o1, o2, o3};
    type = NOPPY;
    for (int i = 0; i < 4; i++) {
        if (ops[i] != null_op && (out_mask & (1 << i))) {
            if (ops[i] == 15) {
                type = JUMPY;
                break;
            } else if (ops[i] != 12 && ops[i] != 9) {
                type = BAD;
            }
        }
    }
}
NOINLINE UNUSED
static void P_pcrel(UNUSED struct tc *ctx, uint32_t dpc,
                    UNUSED struct arch_pcrel_info info) {
    return P_data(ctx, info.reg, null_op, null_op, null_op, 1);
}
NOINLINE UNUSED
static void P_thumb_it(UNUSED struct tc *ctx) {
    type = NOPPY;
}

NOINLINE UNUSED
static void P_ret(UNUSED struct tc *ctx) {
    type = JUMPY;
}

NOINLINE UNUSED
static void P_indirect_call(UNUSED struct tc *ctx) {
    type = JUMPY;
}

NOINLINE UNUSED
static void P_branch(UNUSED struct tc *ctx, uint64_t dpc, int cc) {
    type = BAD;
}

NOINLINE UNUSED
static void P_unidentified(UNUSED struct tc *ctx) {
    type = BAD;
}

NOINLINE UNUSED
static void P_bad(UNUSED struct tc *ctx) {
    type = JUMPY;
}

#include "arm/dis-main.inc.h"

int main(UNUSED int argc, char **argv) {
    struct tc ctx;
    ctx.base.pc = 0xdead0000;
    memset(ctx.base.newop, 0, sizeof(ctx.base.newop));
    ctx.base.modify = false;
    for (uint32_t hi = 0; hi < (1 << 12); hi++) {
        for (uint32_t lo = 0; lo < (1 << 13); lo++) {
            uint32_t op = (0b1111 << 28) | (hi << 16) | (0b111 << 13) | lo;

            if ((op & 0x0f100010) == 0x0e100010)
                continue;
            
            ctx.base.ptr = &op;
            ctx.arch.pc_low_bit = false;
            type = BAD;
            P(dis)(&ctx);
            if (type != JUMPY)
                continue;
            ctx.arch.pc_low_bit = true;
            type = BAD;
            P(dis)(&ctx);
            if (type != NOPPY)
                continue;
            printf("%x\n", op);
        }
    }

}
