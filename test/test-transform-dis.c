#include <stdio.h>
#define TRANSFORM_DIS_VERBOSE 1
#include "transform-dis.c"
#include <stdlib.h>
int main(UNUSED int argc, char **argv) {
    static uint8_t in[1048576];
    UNUSED size_t size = fread(in, 1, sizeof(in), stdin);
    int patch_size = atoi(argv[1]);
    struct arch_dis_ctx arch;
    arch_dis_ctx_init(&arch);
#ifdef TARGET_arm
    int thumb = atoi(argv[2]);
    arch.pc_low_bit = thumb;
#endif
    uint8_t out[patch_size * 10];
    /* patch_size bytes of patch
     * max 2 bytes of tail
     * max 12 more bytes of ITted insns
     * 1 off-by-one written to simplify the code */
    int offsets[patch_size + 15];
    void *rewritten_ptr = out;
    printf("\n#if 0\n");
    uint_tptr pc_patch_start = 0x10000;
    uint_tptr pc_patch_end = pc_patch_start + patch_size;
    int ret = transform_dis_main(
        in,
        &rewritten_ptr,
        pc_patch_start,
        &pc_patch_end,
        &arch,
        offsets);
    printf("=> %d\n", ret);
    printf("#endif\n");
    int print_out_idx = 0;
    int print_in_idx = 0;
    if (!ret) {
        printf("// total length: %zd\n", (uint8_t *) rewritten_ptr - out);
        for(int ii = 0; ii <= (int) (pc_patch_end - pc_patch_start); ii++) {
            int oi = offsets[ii];
            if(oi != -1) {
                int in_size = ii - print_in_idx;
                int out_size = oi - print_out_idx;
                if (in_size != out_size || memcmp(out + print_out_idx, in + print_in_idx, in_size)) {
                    printf("at_%x: nop; nop; nop\n", print_in_idx);
                    printf("   .byte ");
                    while(print_in_idx++ < ii)
                        printf("0x%02x%s", in[print_in_idx-1], print_in_idx == ii ? "" : ", ");
                    printf("\nnop //     -->\n   .byte ");
                    while(print_out_idx++ < oi)
                        printf("0x%02x%s", out[print_out_idx-1], print_out_idx == oi ? "" : ", ");
                    printf("\n");
                }
                print_in_idx = ii;
                print_out_idx = oi;
                printf("/* 0x%x: 0x%x */\n", ii, oi);
            }
        }
    }
}
