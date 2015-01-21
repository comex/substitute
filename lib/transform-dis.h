#pragma once
#include <stdint.h>
#include <stdbool.h>

int transform_dis_main(const void *restrict code_ptr,
                       void **restrict rewritten_ptr_ptr,
                       uintptr_t pc_patch_start,
                       uintptr_t pc_patch_end,
                       struct arch_dis_ctx initial_arch_ctx,
                       int *offset_by_pcdiff);
