#include "substitute-internal.h"
#ifdef TARGET_DIS_SUPPORTED
#include "substitute.h"
#include "jump-dis.h"
#include "transform-dis.h"
#include "execmem.h"
#include "stop-other-threads.h"
#include stringify(TARGET_DIR/jump-patch.h)

struct hook_internal {
    int offset_by_pcdiff[MAX_JUMP_PATCH_SIZE + 1];
    uint8_t jump_patch[MAX_JUMP_PATCH_SIZE];
    size_t jump_patch_size;
    void *code;
    void *outro_trampoline;
    /* page allocated with execmem_alloc_unsealed - only if we had to allocate
     * one when processing this hook */
    void *trampoline_page;
};

struct pc_callback_info {
    struct hook_internal *his;
    size_t nhooks;
    bool encountered_bad_pc;
};

static uintptr_t pc_callback(void *ctx, uintptr_t pc) {
    struct pc_callback_info *restrict info = ctx;
    uintptr_t real_pc = pc;
#ifdef __arm__
    real_pc = pc & ~1;
#endif
    for (size_t i = 0; i < info->nhooks; i++) {
        struct hook_internal *hi = &info->his[i];
        uintptr_t diff = real_pc - (uintptr_t) hi->code;
        if (diff < hi->jump_patch_size) {
            int offset = hi->offset_by_pcdiff[diff];
            if (offset == -1) {
                info->encountered_bad_pc = true;
                return pc;
            }
            return (uintptr_t) hi->outro_trampoline + offset;
        }
    }
    return pc;
}

/* Figure out the size of the patch we need to jump from pc_patch_start
 * to hook->replacement.
 * On ARM, we can jump anywhere in 8 bytes.  On ARM64, we can only do it in two
 * or three instructions if the destination PC is within 4GB or so of the
 * source.  We *could* just brute force it by adding more instructions, but
 * this increases the chance of problems caused by patching too much of the
 * function.  Instead, since we should be able to mmap a trampoline somewhere
 * in that range, we'll stop there on the way to.
 * In order of preference:
 * - Jump directly.
 * - Jump using a trampoline to be placed at our existing trampoline_ptr.
 * - Allocate a new trampoline_ptr, using the target as a hint, and jump there.
 * If even that is out of range, then return an error code.
 */

static int check_intro_trampoline(void **trampoline_ptr_p,
                                  size_t *trampoline_size_left_p,
                                  uintptr_t pc,
                                  uintptr_t dpc,
                                  int *patch_size_p,
                                  bool *need_intro_trampoline_p,
                                  void **trampoline_page_p,
                                  struct arch_dis_ctx arch) {
    void *trampoline_ptr = *trampoline_ptr_p;
    size_t trampoline_size_left = *trampoline_size_left_p;

    /* Try direct */
    *need_intro_trampoline_p = false;
    *patch_size_p = jump_patch_size(pc, dpc, arch, /*force*/ false);
    if (*patch_size_p != -1)
        return SUBSTITUTE_OK;

    /* Try existing trampoline */
    *patch_size_p = jump_patch_size(pc, (uintptr_t) trampoline_ptr, arch, false);

    if (*patch_size_p != -1 && (size_t) *patch_size_p <= *trampoline_size_left_p)
        return SUBSTITUTE_OK;

    /* Allocate new trampoline - try after pc.  If this fails, we can try
     * before pc before giving up. */
    int ret = execmem_alloc_unsealed(pc, &trampoline_ptr, &trampoline_size_left);
    if (ret)
        goto skip_after;

    *patch_size_p = jump_patch_size(pc, (uintptr_t) trampoline_ptr, arch, false);
    if (*patch_size_p != -1) {
        *trampoline_ptr_p = trampoline_ptr;
        *trampoline_size_left_p = trampoline_size_left;
        *trampoline_page_p = trampoline_ptr;
        return SUBSTITUTE_OK;
    }

    execmem_free(trampoline_ptr);

skip_after:;
    /* Allocate new trampoline - try before pc (xxx only meaningful on arm64) */
    uintptr_t start_address = pc - 0xffff0000;
    ret = execmem_alloc_unsealed(start_address, &trampoline_ptr, &trampoline_size_left);
    if (ret)
        return ret;

    *patch_size_p = jump_patch_size(pc, (uintptr_t) trampoline_ptr, arch, false);
    if (*patch_size_p != -1) {
        *trampoline_ptr_p = trampoline_ptr;
        *trampoline_size_left_p = trampoline_size_left;
        *trampoline_page_p = trampoline_ptr;
        return SUBSTITUTE_OK;
    }

    /* I give up... */
    execmem_free(trampoline_ptr);
    return SUBSTITUTE_ERR_OUT_OF_RANGE;
}


EXPORT
int substitute_hook_functions(const struct substitute_function_hook *hooks,
                              size_t nhooks,
                              int options) {
    struct hook_internal *his = malloc(nhooks * sizeof(*his));
    if (!his)
        return SUBSTITUTE_ERR_OOM;

    for (size_t i = 0; i < nhooks; i++)
        his[i].trampoline_page = NULL;

    int ret = SUBSTITUTE_OK;
    ssize_t emw_finished_i = -1;
    bool stopped = false;
    void *stop_token;
    if (!(options & SUBSTITUTE_DONT_STOP_THREADS)) {
        if ((ret = stop_other_threads(&stop_token)))
            goto end;
        stopped = true;
    }

    void *trampoline_ptr = NULL;
    size_t trampoline_size_left = 0;

    /* First run through and (a) ensure all the functions are OK to hook, (b)
     * allocate memory for the trampolines. */
    for (size_t i = 0; i < nhooks; i++) {
        const struct substitute_function_hook *hook = &hooks[i];
        struct hook_internal *hi = &his[i];
        void *code = hook->function;
        struct arch_dis_ctx arch;
        arch_dis_ctx_init(&arch);
#ifdef __arm__
        if ((uintptr_t) code & 1) {
            arch.pc_low_bit = true;
            code--;
        }
#endif
        hi->code = code;
        uintptr_t pc_patch_start = (uintptr_t) code;
        int patch_size;
        bool need_intro_trampoline;
        if ((ret = check_intro_trampoline(&trampoline_ptr, &trampoline_size_left,
                                          pc_patch_start, (uintptr_t) hook->replacement,
                                          &patch_size, &need_intro_trampoline,
                                          &hi->trampoline_page, arch)))
            goto end;

        uint_tptr pc_patch_end = pc_patch_start + patch_size;
        /* Generate the rewritten start of the function for the outro
         * trampoline (complaining if any bad instructions are found)
         * (on arm64, this modifies regs_possibly_written, which is used by the
         * two make_jump_patch calls) */
        uint8_t rewritten_temp[TD_MAX_REWRITTEN_SIZE];
        void *rp = rewritten_temp;
        if ((ret = transform_dis_main(code, &rp, pc_patch_start, &pc_patch_end,
                                      &arch, hi->offset_by_pcdiff)))
            goto end;
        /* Check some of the rest of the function for jumps back into the
         * patched region. */
        if ((ret = jump_dis_main(code, pc_patch_start, pc_patch_end, arch)))
            goto end;

        uintptr_t initial_target;
        if (need_intro_trampoline) {
            initial_target = (uintptr_t) trampoline_ptr;
            make_jump_patch(&trampoline_ptr, (uintptr_t) trampoline_ptr,
                            (uintptr_t) hook->replacement, arch);
        } else {
            initial_target = (uintptr_t) hook->replacement;
        }
        void *jp = hi->jump_patch;
        make_jump_patch(&jp, pc_patch_start, initial_target, arch);
        hi->jump_patch_size = (uint8_t *) jp - hi->jump_patch;


        size_t rewritten_size = (uint8_t *) rp - rewritten_temp;
        size_t jumpback_size =
            jump_patch_size((uintptr_t) trampoline_ptr + rewritten_size,
                            pc_patch_end, arch, /* force */ true);
        size_t outro_size = rewritten_size + jumpback_size;
        if (outro_size > trampoline_size_left) {
            /* Not enough space left in our existing block... */
            if ((ret = execmem_alloc_unsealed(0, &trampoline_ptr,
                                              &trampoline_size_left)))
                goto end;
            hi->trampoline_page = trampoline_ptr;
            jumpback_size =
                jump_patch_size((uintptr_t) trampoline_ptr + rewritten_size,
                                pc_patch_end, arch, /* force */ true);
            outro_size = rewritten_size + jumpback_size;
        }

        hi->outro_trampoline = trampoline_ptr;
        uintptr_t dpc = pc_patch_end;
#ifdef __arm__
        if (arch.pc_low_bit) {
            hi->outro_trampoline++;
            dpc++;
        }
#endif
        memcpy(trampoline_ptr, rewritten_temp, rewritten_size);
        trampoline_ptr += rewritten_size;
        make_jump_patch(&trampoline_ptr, (uintptr_t) trampoline_ptr, dpc, arch);
        trampoline_size_left -= outro_size;
    }

    /* Now commit. */
    for (size_t i = 0; i < nhooks; i++) {
        const struct substitute_function_hook *hook = &hooks[i];
        struct hook_internal *hi = &his[i];
        emw_finished_i = (ssize_t) i;
        if ((ret = execmem_write(hi->code, hi->jump_patch, hi->jump_patch_size))) {
            /* User is probably screwed, since this probably means a failure to
             * re-protect exec, thanks to code signing, so now the function is
             * permanently inaccessible. */
            goto end;
        }
        if (hook->old_ptr)
            *(void **) hook->old_ptr = hi->outro_trampoline;
    }

    /* *sigh of relief* now we can rewrite the PCs. */
    if (stopped) {
        struct pc_callback_info info = {his, nhooks, false};
        if ((ret = apply_pc_patch_callback(stop_token, pc_callback, &info)))
            goto end;
        if (info.encountered_bad_pc) {
            ret = SUBSTITUTE_ERR_UNEXPECTED_PC_ON_OTHER_THREAD;
            goto end;
        }
    }

end:
    for (size_t i = 0; i < nhooks; i++) {
        void *page = his[i].trampoline_page;
        if (page) {
            /* if we failed, get rid of the trampolines.  if we succeeded, make
             * them executable */
            if (ret && (ssize_t) i >= emw_finished_i) {
                execmem_free(page);
            } else {
                /* we already patched them all, too late to go back.. */
                ret = execmem_seal(page);
            }
        }
    }
    if (stopped) {
        int r2 = resume_other_threads(stop_token);
        if (!ret)
            ret = r2;
    }
    free(his);
    return ret;
}

#endif /* TARGET_DIS_SUPPORTED */
