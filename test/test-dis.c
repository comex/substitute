#include <stdio.h>
#include "dis.h"
unsigned f(unsigned x) {
    struct bitslice addr = {.nruns = 4, .runs = (struct bitslice_run[]) {{0,0,4}, {5,5,7}, {16,13,4}, {23,12,1}}};
    return bs_get(addr, x);

}
unsigned fs(unsigned val, unsigned op) {
    struct bitslice addr = {.nruns = 4, .runs = (struct bitslice_run[]) {{0,0,4}, {5,5,7}, {16,13,4}, {23,12,1}}};
    return bs_set(addr, val, op);

}
int main() {
    printf("%x\n", f(0xdeadbeef));
    printf("%x\n", f(0xdeadbeee));
    printf("%x\n", f(0xfeedface));
    printf("%x\n", fs(0xdead, 0xdeadbeef));

}
