#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#define IF_BOTHER_WITH_MODIFY(...) __VA_ARGS__
#include "dis.h"

typedef struct tc {
    uint32_t pc;
    void *ptr;
    uint32_t op;
    uint32_t newop;
    uint32_t newval[4];
    bool modify;
    int op_size;
    struct arch_dis_ctx arch;
} *tdis_ctx;
#define P(x) P_##x
#define TDIS_CTX_MODIFY(ctx) ((ctx)->modify)
#define TDIS_CTX_NEWVAL(ctx, n) ((ctx)->newval[n])
#define TDIS_CTX_SET_NEWOP(ctx, new) ((ctx)->newop = (new))

NOINLINE UNUSED
static void P_data(struct tc *ctx, unsigned o0, unsigned o1, unsigned o2, unsigned o3, unsigned out_mask) {
    printf("data: %08x\n", ctx->op);
    unsigned os[] = {o0, o1, o2, o3};
    for(size_t i = 0; i < 4; i++) {
        unsigned val = os[i];
        if(val == -1u)
            break;
        printf("    reg %x: %s\n", val, out_mask & (1 << i) ? "out" : "in");
        ctx->newval[i] = i;
    }
    ctx->modify = true;
}

NOINLINE UNUSED
static void P_pcrel(struct tc *ctx, uint32_t dpc, unsigned reg, enum pcrel_load_mode lm) {
    printf("adr: %08x => %08x r%u lm:%d\n", ctx->op, dpc, reg, lm);
    ctx->modify = false;
}

NOINLINE UNUSED
static void P_ret(struct tc *ctx) {
    printf("ret: %08x\n", ctx->op);
    ctx->modify = false;
}

NOINLINE UNUSED
static void P_branch(struct tc *ctx, uint32_t dpc, bool cond) {
    printf("branch(%s): %08x => %08x\n", cond ? "cond" : "uncond", ctx->op, dpc);
    ctx->modify = false;
}

NOINLINE UNUSED
static void P_unidentified(struct tc *ctx) {
    printf("unidentified: %08x\n", ctx->op);
    ctx->modify = false;
}

NOINLINE UNUSED
static void P_bad(struct tc *ctx) {
    printf("bad: %08x\n", ctx->op);
    ctx->modify = false;
}

#include HDR

#define P_(x) P(x)

int main(UNUSED int argc, char **argv) {
    struct tc ctx;
    ctx.pc = 0xdead0000;
    uint32_t op = strtoll(argv[1] ? argv[1] : "deadbeef", NULL, 16);
    ctx.ptr = &op;
    ctx.newop = 0;
    P_(xdis)(&ctx);
    printf("==> %x (size=%d)\n", ctx.newop, ctx.op_size);

}
