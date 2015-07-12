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

#define u32_hash(up) (*(up) % 100)
#define u32_null(up) (!*(up))
#define u32_eq(u1p, u2p) (*(u1p) == *(u2p))

DECL_STATIC_HTAB_KEY(u32, uint32_t, u32_hash, u32_eq, u32_null, 0);
DECL_HTAB(u32_u32, u32, uint32_t);

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

    HTAB_STORAGE(u32_u32) h2;
    HTAB_STORAGE_INIT(&h2, u32_u32);
    uint32_t h2_raw[501];
    memset(h2_raw, 0xff, sizeof(h2_raw));
    for (int i = 0; i < 3000; i++) {
        uint32_t op = arc4random() & 1;
        uint32_t key = (arc4random() % 500) + 1;
        if (op == 0) { /* set */
            uint32_t val = arc4random() & 0x7fffffff;
            *htab_setp_u32_u32(&h2.h, &key, NULL) = val;
            h2_raw[key] = val;
        } else { /* delete */
            htab_remove_u32_u32(&h2.h, &key);
            h2_raw[key] = -1;
        }
    }
    for (uint32_t k = 1; k <= 500; k++) {
        uint32_t raw = h2_raw[k];
        uint32_t *hashedp = htab_getp_u32_u32(&h2.h, &k);
        uint32_t hashed = hashedp ? *hashedp : -1;
        /* printf("%d %x %x\n", k, raw, hashed); */
        assert(hashed == raw);
    }
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
