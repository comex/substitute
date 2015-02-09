#pragma once
#include <stdint.h>
#include <stdbool.h>
#include stringify(TARGET_DIR/arch-dis.h)

int transform_dis_main(const void *restrict code_ptr,
                       void **restrict rewritten_ptr_ptr,
                       uint_tptr pc_patch_start,
                       uint_tptr *pc_patch_end_p,
                       struct arch_dis_ctx *arch_ctx_p,
                       int *offset_by_pcdiff);
