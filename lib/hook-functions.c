#include "substitute.h"
#include "substitute-internal.h"
#include "execmem.h"
#include "stop-other-threads.h"

static uintptr_t patch_callback(UNUSED void *ctx, uintptr_t pc) {
    printf("patch_callback: pc=%llx\n", (long long) pc);
    return pc;
}

EXPORT
int substitute_hook_functions(const struct substitute_function_hook *hooks,
                              size_t nhooks,
                              int options) {
    (void) hooks; (void) nhooks;
    int ret = SUBSTITUTE_OK;
    void *stop_token;
    if (!(options & SUBSTITUTE_DONT_STOP_THREADS)) {
        if ((ret = stop_other_threads(&stop_token)))
            return ret;
    }
    if (!(options & SUBSTITUTE_DONT_STOP_THREADS)) {
        if ((ret = apply_pc_patch_callback(stop_token, patch_callback, NULL)))
            goto fail;
    }

fail:
    if (!(options & SUBSTITUTE_DONT_STOP_THREADS)) {
        int r2 = resume_other_threads(stop_token);
        if (!ret)
            ret = r2;
    }
    return ret;
}
