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

#if defined(TARGET_x86_64) || defined(TARGET_i386)
NOINLINE UNUSED
static void P_pcrel(UNUSED struct tc *ctx, uint32_t dpc,
                    UNUSED struct arch_pcrel_info info) {
    printf("adr => %08x\n", dpc);
}
#else
NOINLINE UNUSED
static void P_data(UNUSED struct tc *ctx, unsigned o0, unsigned o1, unsigned o2,
                   unsigned o3, unsigned out_mask) {
    printf("data\n");
    unsigned os[] = {o0, o1, o2, o3};
    for(size_t i = 0; i < 4; i++) {
        unsigned val = os[i];
        if(val == -1u)
            break;
        printf("    reg %x: %s\n", val, out_mask & (1 << i) ? "out" : "in");
        ctx->base.newval[i] = i;
    }
    ctx->base.modify = true;
}
NOINLINE UNUSED
static void P_pcrel(UNUSED struct tc *ctx, uint32_t dpc,
                    struct arch_pcrel_info info) {
    printf("adr => %08x r%u lm:%d\n", dpc, info.reg, info.load_mode);
}
NOINLINE UNUSED
static void P_thumb_it(UNUSED struct tc *ctx) {
    printf("thumb_it\n");
}
#endif

NOINLINE UNUSED
static void P_ret(UNUSED struct tc *ctx) {
    printf("ret\n");
}

NOINLINE UNUSED
static void P_indirect_call(UNUSED struct tc *ctx) {
    printf("indirect call\n");
}

NOINLINE UNUSED
static void P_branch(UNUSED struct tc *ctx, uint64_t dpc, int cc) {
    printf("branch(%s,%s) => %08llx\n",
           (cc & CC_CONDITIONAL) ? "cond" : "uncond",
           (cc & CC_CALL) ? "call" : "!call",
           (unsigned long long) dpc);
}

NOINLINE UNUSED
static void P_unidentified(UNUSED struct tc *ctx) {
    printf("unidentified\n");
}

NOINLINE UNUSED
static void P_bad(UNUSED struct tc *ctx) {
    printf("bad\n");
}

#include HDR

#define P_(x) P(x)

int main(UNUSED int argc, char **argv) {
    struct tc ctx;
    ctx.base.pc = 0xdead0000;
    const char *op_str = argv[1];
#if defined(TARGET_x86_64) || defined(TARGET_i386)
    uint8_t op[20] = {0};
    if (!op_str)
        op_str = "deadbeef";
    size_t len = strlen(op_str);
    if (len % 1 || len > 32) {
        printf("bad op_str len\n");
        return 1;
    }
    for (size_t i = 0; i < len; i += 2) {
        char str[3] = {op_str[i], op_str[i+1], 0};
        char *end;
        uint8_t byte = strtol(str, &end, 16);
        if (*end) {
            printf("bad op_str byte\n");
            return 1;
        }
        op[i/2] = byte;
    }
    ctx.base.ptr = op;
    ctx.base.modify = false;
    P_(xdis)(&ctx);
    printf("(size=%d/%zd)\n", ctx.base.op_size, len / 2);
#else
    uint32_t op = strtoll(op_str ? op_str : "deadbeef", NULL, 16);
    ctx.base.ptr = &op;
    memset(ctx.base.newop, 0, sizeof(ctx.base.newop));
    ctx.base.modify = false;
    printf("%08x: ", op);
    P_(xdis)(&ctx);
    printf("==> %x (size=%d)\n", *(uint32_t *) ctx.base.newop, ctx.base.op_size);
#endif

}
