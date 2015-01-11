#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "dis.h"

typedef void tdis_ret;
typedef struct tc {
    uint32_t pc;
    uint32_t op;

} *tdis_ctx;
#define P(x) P_##x

__attribute__((noinline))
static void P_data(struct tc *ctx, struct dis_data_operand *ops, size_t nops) {
    printf("data: %08x\n", ctx->op);
    for(size_t i = 0; i < nops; i++) {
        const struct bitslice *n = ops[i].n;
        unsigned val = bs_get(n, ctx->op);
        printf("    {");
        for(int j = 0; j < n->nruns; j++) {
            struct bitslice_run run = n->runs[j];
            printf(" %d:%d:%d", run.inpos, run.outpos, run.len);
        }
        printf(" } =>\n");
        printf("    reg %x: %s\n", val, ops[i].out ? "out" : "in");
    }
    unsigned newop = ctx->op;
    for(size_t i = 0; i < nops; i++)
        newop = bs_set(ops[i].n, i, newop);
    printf("modified: %x\n", newop);
}

static void P_adr(struct tc *ctx, UNUSED uint32_t dpc) {
    printf("adr: %08x\n", ctx->op);
}

static void P_branch(struct tc *ctx, UNUSED uint32_t dpc) {
    printf("branch: %08x\n", ctx->op);
}

static void P_unidentified(struct tc *ctx) {
    printf("unidentified: %08x\n", ctx->op);

}
#include "dis-arm.inc.h"

int main(UNUSED int argc, char **argv) {
    struct tc ctx;
    ctx.pc = 0xdead0000;
    ctx.op = (uint32_t) strtoll(argv[1] ? argv[1] : "deadbeef", NULL, 16);
    P_dis_arm(&ctx);

}
