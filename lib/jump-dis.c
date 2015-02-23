#include "substitute-internal.h"
#ifdef TARGET_DIS_SUPPORTED
#define DIS_MAY_MODIFY 0
#include "dis.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* This pass tries to look through the function to find jumps back to the
 * patched code at the beginning to the function.  It does not deal with jump
 * tables, and has a limited range, so it is only heuristic.  If such jumps are
 * found, the hook is aborted.  In the future it might be possible to fix up
 * the jumps rather than merely detect them, but that would require doing
 * something weird like extending the patch region to add trampolines... */

enum {
    JUMP_ANALYSIS_MAX_INSNS = 512,
    JUMP_ANALYSIS_MAX_SIZE = JUMP_ANALYSIS_MAX_INSNS * MIN_INSN_SIZE,
};

struct jump_dis_ctx {
    /* outputs */
    bool bad_insn;
    bool continue_after_this_insn;

    struct dis_ctx_base base;

    uint_tptr pc_patch_start;
    uint_tptr pc_patch_end;

    uint8_t seen_mask[JUMP_ANALYSIS_MAX_INSNS / 8];
    /* queue of instructions to visit */
    uint_tptr *queue;
    size_t queue_write_off;
    size_t queue_read_off;
    size_t queue_size;
    size_t queue_count;

    struct arch_dis_ctx arch;
};

#undef P
#define P(x) jump_dis_##x

#define tdis_ctx struct jump_dis_ctx *

static void jump_dis_add_to_queue(struct jump_dis_ctx *ctx, uint_tptr pc) {
    size_t diff = (pc - ctx->pc_patch_start) / MIN_INSN_SIZE;
    if (diff >= JUMP_ANALYSIS_MAX_INSNS) {
#ifdef JUMP_DIS_VERBOSE
        printf("jump-dis: not adding %llx - out of range\n", (unsigned long long) pc);
#endif
        return;
    }
    if (ctx->seen_mask[diff / 8] & 1 << (diff % 8)) {
#ifdef JUMP_DIS_VERBOSE
        printf("jump-dis: not adding %llx - already seen\n", (unsigned long long) pc);
#endif
        return;
    }
    ctx->seen_mask[diff / 8] |= 1 << (diff % 8);

    if (ctx->queue_write_off == ctx->queue_read_off && (ctx->queue_count || !ctx->queue_size)) {
        size_t new_size = ctx->queue_size * 2 + 5;
        ctx->queue = realloc(ctx->queue, new_size * sizeof(*ctx->queue));
        if (!ctx->queue)
            substitute_panic("%s: out of memory\n", __func__);
        size_t new_read_off = new_size - (ctx->queue_size - ctx->queue_read_off);
        memmove(ctx->queue + new_read_off,
                ctx->queue + ctx->queue_read_off,
                (ctx->queue_size - ctx->queue_read_off) * sizeof(*ctx->queue));
        ctx->queue_read_off = new_read_off % new_size;
        ctx->queue_size = new_size;
    }
    ctx->queue[ctx->queue_write_off] = pc;
    ctx->queue_write_off = (ctx->queue_write_off + 1) % ctx->queue_size;
    ctx->queue_count++;
}

static INLINE UNUSED
void jump_dis_data(UNUSED struct jump_dis_ctx *ctx,
                   UNUSED unsigned o0, UNUSED unsigned o1, UNUSED unsigned o2,
                   UNUSED unsigned o3, UNUSED unsigned out_mask) {
    /* on ARM, ignore mov PC jumps, as they're unlikely to be in the same function */
}

static INLINE UNUSED
void jump_dis_pcrel(struct jump_dis_ctx *ctx, uint_tptr dpc,
                    UNUSED struct arch_pcrel_info info) {
    ctx->bad_insn = dpc >= ctx->pc_patch_start && dpc < ctx->pc_patch_end;
}

static INLINE UNUSED
void jump_dis_indirect_call(UNUSED struct jump_dis_ctx *ctx) {
}

static INLINE UNUSED
void jump_dis_ret(struct jump_dis_ctx *ctx) {
    ctx->continue_after_this_insn = false;
}

static NOINLINE UNUSED
void jump_dis_branch(struct jump_dis_ctx *ctx, uint_tptr dpc, int cc) {
    if (dpc >= ctx->pc_patch_start && dpc < ctx->pc_patch_end) {
        ctx->bad_insn = true;
        return;
    }
#ifdef JUMP_DIS_VERBOSE
    printf("jump-dis: enqueueing %llx\n", (unsigned long long) dpc);
#endif
    jump_dis_add_to_queue(ctx, dpc);
    ctx->continue_after_this_insn = cc & (CC_CONDITIONAL | CC_CALL);
}

static INLINE UNUSED
void jump_dis_unidentified(UNUSED struct jump_dis_ctx *ctx) {
}

static INLINE UNUSED
void jump_dis_bad(struct jump_dis_ctx *ctx) {
    ctx->continue_after_this_insn = false;
}

static INLINE UNUSED
void jump_dis_thumb_it(UNUSED struct jump_dis_ctx *ctx) {
}

static void jump_dis_dis(struct jump_dis_ctx *ctx);

bool jump_dis_main(void *code_ptr, uint_tptr pc_patch_start, uint_tptr pc_patch_end,
                   struct arch_dis_ctx initial_dis_ctx) {
    bool ret;
    struct jump_dis_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.pc_patch_start = pc_patch_start;
    ctx.pc_patch_end = pc_patch_end;
    ctx.base.pc = pc_patch_end;
    ctx.arch = initial_dis_ctx;
    while (1) {
        ctx.bad_insn = false;
        ctx.continue_after_this_insn = true;
        ctx.base.ptr = code_ptr + (ctx.base.pc - pc_patch_start);
        jump_dis_dis(&ctx);
#ifdef JUMP_DIS_VERBOSE
        printf("jump-dis: pc=%llx op=%08x size=%x bad=%d continue_after=%d\n",
            (unsigned long long) ctx.base.pc,
            ctx.base.op,
            ctx.base.op_size,
            ctx.bad_insn,
            ctx.continue_after_this_insn);
#endif
        if (ctx.bad_insn) {
            ret = true;
            goto fail;
        }
        if (ctx.continue_after_this_insn)
            jump_dis_add_to_queue(&ctx, ctx.base.pc + ctx.base.op_size);

        /* get next address */
        if (ctx.queue_read_off == ctx.queue_write_off)
            break;
        ctx.base.pc = ctx.queue[ctx.queue_read_off];
        ctx.queue_read_off = (ctx.queue_read_off + 1) % ctx.queue_size;
        ctx.queue_count--;
    }
    /* no bad instructions! */
    ret = false;
fail:
    free(ctx.queue);
    return ret;
}

#include stringify(TARGET_DIR/dis-main.inc.h)
#endif /* TARGET_DIS_SUPPORTED */
