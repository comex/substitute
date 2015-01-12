#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#define IF_BOTHER_WITH_MODIFY(...) __VA_ARGS__
#include "dis.h"

typedef struct { bool modify; } tdis_ret;
typedef struct tc {
    uint32_t pc;
    uint32_t op;
    uint32_t newop;
    uint32_t newval[4];

} *tdis_ctx;
#define P(x) P_##x

NOINLINE UNUSED
static tdis_ret P_data(struct tc *ctx, unsigned o0, unsigned o1, unsigned o2, unsigned o3, unsigned out_mask) {
    printf("data: %08x\n", ctx->op);
    unsigned os[] = {o0, o1, o2, o3};
    for(size_t i = 0; i < 4; i++) {
        unsigned val = os[i];
        if(val == -1u)
            break;
        printf("    reg %x: %s\n", val, out_mask & (1 << i) ? "out" : "in");
        ctx->newval[i] = i;
    }
    return (tdis_ret) {true};
}

NOINLINE UNUSED
static tdis_ret P_pcrel(struct tc *ctx, uint32_t dpc, unsigned reg, bool is_load) {
    printf("adr%s: %08x => %08x r%u\n", is_load ? "+load" : "", ctx->op, dpc, reg);
    return (tdis_ret) {false};
}

NOINLINE UNUSED
static tdis_ret P_ret(struct tc *ctx) {
    printf("ret: %08x\n", ctx->op);
    return (tdis_ret) {false};
}

NOINLINE UNUSED
static tdis_ret P_branch(struct tc *ctx, uint32_t dpc) {
    printf("branch: %08x => %08x\n", ctx->op, dpc);
    return (tdis_ret) {false};
}

NOINLINE UNUSED
static tdis_ret P_unidentified(struct tc *ctx) {
    printf("unidentified: %08x\n", ctx->op);
    return (tdis_ret) {false};
}

NOINLINE UNUSED
static tdis_ret P_bad(struct tc *ctx) {
    printf("bad: %08x\n", ctx->op);
    return (tdis_ret) {false};
}

#include HDR

int main(UNUSED int argc, char **argv) {
    struct tc ctx;
    ctx.pc = 0xdead0000;
    ctx.op = (uint32_t) strtoll(argv[1] ? argv[1] : "deadbeef", NULL, 16);
    ctx.newop = 0;
    PDIS(&ctx);
    printf("==> %x\n", ctx.newop);

}
