#include "substitute.h"
#include "dis.h"
#include <stdbool.h>
#include <stdint.h>

#undef P
#define P(x) transform_dis_##x
struct transform_dis_ctx {
    /* outputs */
    bool modify;
    int err;

    bool pc_low_bit;
    uintptr_t pc_patch_start;
    uintptr_t pc_patch_end;
    uintptr_t pc;
    int op_size;
    unsigned op;
    unsigned newop;
    unsigned newval[4];

    const void *ptr;
    void **rewritten_ptr_ptr;
    void *write_newop_here;
};

#define tdis_ctx struct transform_dis_ctx *
#define TDIS_CTX_MODIFY(ctx) ((ctx)->modify)
#define TDIS_CTX_NEWVAL(ctx, n) ((ctx)->newval[n])
#define TDIS_CTX_SET_NEWOP(ctx, new) ((ctx)->newop = (new))

/* largely similar to jump_dis */

static INLINE UNUSED void transform_dis_ret(struct transform_dis_ctx *ctx) {
    /* ret is okay if it's at the end of the patch */
    if (ctx->pc + ctx->op_size < ctx->pc_patch_end)
        ctx->err = SUBSTITUTE_ERR_FUNC_TOO_SHORT;
}

static INLINE UNUSED void transform_dis_branch(struct transform_dis_ctx *ctx,
        uintptr_t dpc, UNUSED bool conditional) {
    if (dpc >= ctx->pc_patch_start && dpc < ctx->pc_patch_end) {
        /* don't support this for now */
        ctx->err = SUBSTITUTE_ERR_FUNC_BAD_INSN_AT_START;
    }
    /* branch out of bounds is fine */
}

static INLINE UNUSED void transform_dis_unidentified(UNUSED struct transform_dis_ctx *ctx) {
    /* this isn't exhaustive, so unidentified is fine */
}

static INLINE UNUSED void transform_dis_bad(struct transform_dis_ctx *ctx) {
    ctx->err = SUBSTITUTE_ERR_FUNC_BAD_INSN_AT_START;
}


/* provide arch-specific definitions of these */
static void transform_dis_data(struct transform_dis_ctx *ctx, unsigned o0,
    unsigned o1, unsigned o2, unsigned o3, unsigned out_mask);
static void transform_dis_pcrel(struct transform_dis_ctx *ctx, uintptr_t dpc,
    unsigned reg, enum pcrel_load_mode mode);

static void transform_dis_dis(struct transform_dis_ctx *ctx);

int transform_dis_main(const void *restrict code_ptr,
                       void **restrict rewritten_ptr_ptr,
                       uintptr_t pc_patch_start,
                       uintptr_t pc_patch_end,
                       bool pc_low_bit,
                       int *offset_by_pcdiff) {
    struct transform_dis_ctx ctx;
    ctx.pc_patch_start = pc_patch_start;
    ctx.pc_patch_end = pc_patch_end;
    ctx.pc_low_bit = pc_low_bit;
    ctx.pc = pc_patch_start;
    /* data is written to rewritten both by this function directly and, in case
     * additional scaffolding is needed, by arch-specific transform_dis_* */
    ctx.rewritten_ptr_ptr = rewritten_ptr_ptr;
    void *rewritten_start = *rewritten_ptr_ptr;
    int written_pcdiff = 0;
    while (ctx.pc < ctx.pc_patch_end) {
        ctx.modify = false;
        ctx.err = 0;
        ctx.newop = ctx.op;
        ctx.ptr = code_ptr + (ctx.pc - pc_patch_start);
        void *rewritten_ptr = *rewritten_ptr_ptr;
        ctx.write_newop_here = rewritten_ptr;
        transform_dis_dis(&ctx);

        if (ctx.err)
            return ctx.err;
        if (ctx.write_newop_here != NULL) {
            if (ctx.op_size == 4)
                *(uint32_t *) ctx.write_newop_here = ctx.newop;
            else if (ctx.op_size == 2)
                *(uint16_t *) ctx.write_newop_here = ctx.newop;
            else
                __builtin_abort();
            if (*rewritten_ptr_ptr == rewritten_ptr)
                *rewritten_ptr_ptr += ctx.op_size;
        }
        ctx.pc += ctx.op_size;
        int pcdiff = ctx.pc - ctx.pc_patch_start;
        while (written_pcdiff < pcdiff)
            offset_by_pcdiff[written_pcdiff++] = -1;
        offset_by_pcdiff[written_pcdiff++] = (int) (*rewritten_ptr_ptr - rewritten_start);
    }
    return SUBSTITUTE_OK;
}

static inline void op32(struct transform_dis_ctx *ctx, uint32_t op){ 
    void **rpp = ctx->rewritten_ptr_ptr;
    *(uint32_t *) *rpp = op;
    *rpp += 4;
}

#include TARGET_DIS_HEADER
