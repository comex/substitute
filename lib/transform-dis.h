#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "dis.h"

#define TRANSFORM_DIS_BAN_CALLS 1
#define TRANSFORM_DIS_REL_JUMPS 2

int transform_dis_main(const void *restrict code_ptr,
                       void **restrict rewritten_ptr_ptr,
                       uint_tptr pc_patch_start,
                       uint_tptr *pc_patch_end_p,
                       uint_tptr pc_trampoline,
                       struct arch_dis_ctx *arch_ctx_p,
                       int *offset_by_pcdiff,
                       int options);
