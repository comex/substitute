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

   uintptr_t pc;
   uintptr_t pc_patch_start;
   uintptr_t pc_patch_end;
   bool pc_low_bit;
   unsigned op;
   void *ptr;
   int op_size;
   uint8_t seen_mask[JUMP_ANALYSIS_MAX_INSNS / 8];
   /* queue of instructions to visit */
   uintptr_t *queue;
   size_t queue_write_off;
   size_t queue_read_off;
   size_t queue_size;
   size_t queue_count;
};

#define tdis_ctx struct jump_dis_ctx *
#define TDIS_CTX_MODIFY(ctx) 0
#define TDIS_CTX_NEWVAL(ctx, n) 0
#define TDIS_CTX_SET_NEWOP(ctx, new) ((void) 0)

static void P(add_to_queue)(struct jump_dis_ctx *ctx, uintptr_t pc) {
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
        memmove(ctx->queue + new_read_off, ctx->queue + ctx->queue_read_off, (ctx->queue_size - ctx->queue_read_off) * sizeof(*ctx->queue));
        ctx->queue_read_off = new_read_off % new_size;
        ctx->queue_size = new_size;
    }
    ctx->queue[ctx->queue_write_off] = pc;
    ctx->queue_write_off = (ctx->queue_write_off + 1) % ctx->queue_size;
    ctx->queue_count++;
}


static INLINE inline void P(data)(UNUSED struct jump_dis_ctx *ctx, UNUSED unsigned o0, UNUSED unsigned o1, UNUSED unsigned o2, UNUSED unsigned o3, UNUSED unsigned out_mask) {
    /* on ARM, ignore mov PC jumps, as they're unlikely to be in the same function */
}

static INLINE inline void P(pcrel)(struct jump_dis_ctx *ctx, uintptr_t dpc, UNUSED unsigned reg, UNUSED bool is_load) {
    ctx->bad_insn = dpc >= ctx->pc_patch_start && dpc < ctx->pc_patch_end;
}

NOINLINE UNUSED
static void P(ret)(struct jump_dis_ctx *ctx) {
    ctx->continue_after_this_insn = false;
}

NOINLINE UNUSED
static void P(branch)(struct jump_dis_ctx *ctx, uintptr_t dpc, bool conditional) {
    if (dpc >= ctx->pc_patch_start && dpc < ctx->pc_patch_end) {
        ctx->bad_insn = true;
        return;
    }
#ifdef JUMP_DIS_VERBOSE
    printf("jump-dis: enqueueing %llx\n", (unsigned long long) dpc);
#endif
    P(add_to_queue)(ctx, dpc);
    ctx->continue_after_this_insn = conditional;
}

NOINLINE UNUSED
static void P(unidentified)(UNUSED struct jump_dis_ctx *ctx) {
}

NOINLINE UNUSED
static void P(bad)(struct jump_dis_ctx *ctx) {
    ctx->continue_after_this_insn = false;
}

static void P(dis)(tdis_ctx ctx);

bool P(main)(void *code_ptr, uintptr_t pc_patch_start, uintptr_t pc_patch_end, bool pc_low_bit) {
    bool ret;
    struct jump_dis_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.pc_patch_start = pc_patch_start;
    ctx.pc_patch_end = pc_patch_end;
    ctx.pc_low_bit = pc_low_bit;
    ctx.pc = pc_patch_end;
    while (1) {
        ctx.bad_insn = false;
        ctx.continue_after_this_insn = true;
        ctx.ptr = code_ptr + (ctx.pc - pc_patch_start);
        P(dis)(&ctx);
#ifdef JUMP_DIS_VERBOSE
        printf("jump-dis: pc=%llx op=%08x size=%x bad=%d continue_after=%d\n",
            (unsigned long long) ctx.pc,
            ctx.op,
            ctx.op_size,
            ctx.bad_insn,
            ctx.continue_after_this_insn);
#endif
        if (ctx.bad_insn) {
            ret = true;
            goto fail;
        }
        if (ctx.continue_after_this_insn)
            P(add_to_queue)(&ctx, ctx.pc + ctx.op_size);

        /* get next address */
        if (ctx.queue_read_off == ctx.queue_write_off)
            break;
        ctx.pc = ctx.queue[ctx.queue_read_off];
        ctx.queue_read_off = (ctx.queue_read_off + 1) % ctx.queue_size;
        ctx.queue_count--;
    }
    /* no bad instructions! */
    ret = false;
fail:
    free(ctx.queue);
    return ret;
}
