#include "substitute-internal.h"
#include "execmem.h"
#include <stdio.h>
#include <search.h> /* for the victim */
#include <errno.h>
#define NOP_SLED \
   __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;" \
                    "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;" \
                    "nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;")
#define OTHER_SIZE 32 /* guess */

int other(size_t a) {
   if (__builtin_expect(!a, 1))
      return 6;
   NOP_SLED;
   return 0;
}

__attribute__((section("__TEST,__test"), noinline))
int test(size_t a) {
   NOP_SLED;
   if (!a)
      return 5;
   else
      return 1000;
}

static int ewrite(void *dst, const void *src, size_t len) {
   struct execmem_foreign_write w = {dst, src, len};
   return execmem_foreign_write_with_pc_patch(&w, 1, NULL, NULL);
}

int main() {
   printf("this should be 5: %d\n", test(0));
   printf("=> %d\n", ewrite(test, other, OTHER_SIZE));
   printf("   %s\n", strerror(errno));
   printf("this should be 6: %d\n", test(0));
   printf("=> %d\n", ewrite(hcreate, other, OTHER_SIZE));
   printf("   %s\n", strerror(errno));
   printf("modified shared cache func should be 6: %d\n", hcreate(0));

}
