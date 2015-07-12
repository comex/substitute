#include "cbit/htab.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
struct teststr {
    bool valid;
    const char *what;
};
#define ts_null(ts) ({ if (0) printf("null? %p\n", *ts); !*ts; })
#define ts_eq(ts, cp) ({ if (0) printf("eq? %p %p\n", *ts, *cp); !strcmp(*(ts), *(cp)); })
#define ts_hash(strp) strlen(*(strp))
DECL_EXTERN_HTAB_KEY(teststr, const char *);
DECL_HTAB(teststr_int, teststr, int);

int main() {
    /* test loop crap */
    LET(int y = 5)
        printf("5=%d\n", y);
    for (int i = 0; ; i++) {
        LET_LOOP(int x = 5) {
            printf("*%d.%d\n", i, x);
            if (i == 4)
                break;
            else
                continue;
        }
        abort();
    }


    struct htab_teststr_int *hp;
    HTAB_STORAGE(teststr_int) stor = HTAB_STORAGE_INIT_STATIC(&stor, teststr_int);
    /*HTAB_STORAGE_INIT(&stor, teststr_int);*/
    hp = &stor.h;
    for(int i = 0; i < 100; i++) {
        const char *k;
        asprintf((char **) &k, "foo%d", i);
        bool new;
        *htab_setp_teststr_int(hp, &k, &new) = i;
        assert(new);
        assert(htab_getbucket_teststr_int(hp, &k)->value == i);
        assert(*htab_getp_teststr_int(hp, &k) == i);
    }
    {
        const char *k = "foo31";
        bool new;
        htab_setp_teststr_int(hp, &k, NULL);
        htab_setp_teststr_int(hp, &k, &new);
        assert(!new);
        htab_remove_teststr_int(hp, &k);
    }
    HTAB_FOREACH(hp, const char **k, int *v, teststr_int) {
        if(*v % 10 == 1)
            printf("%s -> %d\n", *k, *v);
    }
    htab_free_storage_teststr_int(hp);
}

/*
expect-output<<
foo91 -> 91
foo21 -> 21
foo1 -> 1
foo11 -> 11
foo31 -> 31
foo41 -> 41
foo51 -> 51
foo61 -> 61
foo71 -> 71
foo81 -> 81
>>
expect-exit 0
*/

IMPL_HTAB_KEY(teststr, ts_hash, ts_eq, ts_null, /*nil_byte*/ 0);
