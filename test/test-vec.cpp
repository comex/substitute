#include "cbit/vec.h"
#include <stdio.h>
DECL_VEC(const char *, ccp);
DECL_VEC(int, int);

int main() {
    VEC_STORAGE(ccp) stor = VEC_STORAGE_INIT_STATIC(&stor, cpp);
    VEC_STORAGE_INIT(&stor, ccp);
    struct vec_ccp *vec = &stor.v;
    for (int i = 0; i < 20; i++) {
        char *x;
        asprintf(&x, "el%d", i);
        vec_append_ccp(vec, x);
    }
    vec_remove_ccp(vec, 5, 4);
    vec_add_space_ccp(vec, 9, 2);
    for (int i = 9; i <= 10; i++)
        vec->els[i] = "TILT";
    vec_concat_ccp(vec, vec);
    VEC_FOREACH(vec, i, const char **c, ccp) {
        printf("%zd->%s\n", i, *c);
    }

}
