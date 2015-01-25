#pragma once
#include <stdint.h>

/* Stop the world; return token to be used for applying PC patches and resuming. */
int stop_other_threads(void **token_ptr);
int apply_pc_patch_callback(void *token,
                            uintptr_t (*pc_patch_callback)(void *ctx, uintptr_t pc),
                            void *ctx);
int resume_other_threads(void *token);
