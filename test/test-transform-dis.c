#include <stdio.h>
#define TRANSFORM_DIS_VERBOSE 1
#include "transform-dis.c"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

_Noreturn
static void usage() {
    printf("usage: test-transform-dis (manual patch_size | auto) <thumb if arm>\n");
    exit(1);
}

static void do_manual(uint8_t *in, size_t in_size, int patch_size,
                      struct arch_dis_ctx arch) {
    (void) in_size;
    /* on ARM:
     *    patch_size bytes of patch
     *    max 2 bytes of tail
     *    max 12 more bytes of ITted insns
     * on x86:
     *    max 14 bytes of tail
     * everywhere:
     *    1 off-by-one written to simplify the code
     */
    int offsets[patch_size + 15];
    uint8_t out[patch_size * 10];
    void *rewritten_ptr = out;
    printf("\n#if 0\n");
    uint_tptr pc_patch_start = 0x10000;
    uint_tptr pc_patch_end = pc_patch_start + patch_size;
    uint_tptr pc_trampoline = 0xf000;
    int ret = transform_dis_main(
        in,
        &rewritten_ptr,
        pc_patch_start,
        &pc_patch_end,
        pc_trampoline,
        &arch,
        offsets,
        TRANSFORM_DIS_BAN_CALLS);
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

static void hex_dump(const uint8_t *buf, size_t size) {
    printf("  .byte ");
    for (size_t i = 0; i < size; i++) {
        if (i)
            printf(", ");
        printf("0x%02x", buf[i]);
    }
    printf("\n");
}

static void print_given(const uint8_t *given, size_t given_size) {
    printf("given:\n");
    hex_dump(given, given_size);
}

static void do_auto(uint8_t *in, size_t in_size, struct arch_dis_ctx arch) {
    uint8_t *end = in + in_size;
    assert(!memcmp(in, "GIVEN", 5)); in += 5;
    while (in < end) {
        uint8_t *given = in;
        uint8_t *expect = memmem(in, end - in, "EXPECT", 6);
        assert(expect);
        size_t given_size = expect - given;
        expect += 6;
        in = expect;
        bool expect_err = false;
        size_t expect_size;
        if (!memcmp(expect, "_ERR", 4)) {
            expect_err = true;
            in += 4;
            if (in != end) {
                assert(!memcmp(in, "GIVEN", 5));
                in += 5;
            }
        } else {
            in = memmem(in, end - in, "GIVEN", 5);
            if (in) {
                expect_size = in - expect;
                in += 5;
            } else {
                in = end;
                expect_size = in - expect;
            }
        }
        size_t patch_size = given_size;
        int offsets[patch_size + 15];
        uint8_t out[patch_size * 10];
        void *rewritten_ptr = out;
        uint_tptr pc_patch_start = 0xdead0000;
        uint_tptr pc_patch_end = pc_patch_start + patch_size;
        uint_tptr pc_trampoline = 0xdeac0000;
        int ret = transform_dis_main(
            given,
            &rewritten_ptr,
            pc_patch_start,
            &pc_patch_end,
            pc_trampoline,
            &arch,
            offsets,
            0);//TRANSFORM_DIS_BAN_CALLS);
        if (ret) {
            if (expect_err) {
                printf("OK\n");
            } else {
                print_given(given, given_size);
                printf("got ret %d, expected success\n\n", ret);
            }
        } else {
            if (expect_err) {
                print_given(given, given_size);
                printf("got OK, expected error\n\n");
            } else if (rewritten_ptr != out + expect_size ||
                       memcmp(out, expect, expect_size)) {
                print_given(given, given_size);
                printf("got:\n");
                hex_dump(out, (uint8_t *) rewritten_ptr - out);
                printf("but expected:\n");
                hex_dump(expect, expect_size);
                printf("\n");
            } else {
                printf("OK\n");
            }
        }

    }

}

int main(UNUSED int argc, char **argv) {
    argv++;
    if (!*argv)
        usage();
    enum { MANUAL, AUTO } mode;
    if (!strcmp(*argv, "manual"))
        mode = MANUAL;
    else if (!strcmp(*argv, "auto"))
        mode = AUTO;
    else
        usage();
    argv++;

    int patch_size;
    if (mode == MANUAL) {
        if (!*argv)
            usage();
        patch_size = atoi(*argv++);
    }

    struct arch_dis_ctx arch;
    arch_dis_ctx_init(&arch);
#ifdef TARGET_arm
    if (!*argv)
        usage();
    int thumb = atoi(*argv++);
    arch.pc_low_bit = thumb;
#endif


    static uint8_t in[1048576];
    size_t in_size = fread(in, 1, sizeof(in), stdin);

    if (mode == MANUAL)
        do_manual(in, in_size, patch_size, arch);
    else
        do_auto(in, in_size, arch);
}
