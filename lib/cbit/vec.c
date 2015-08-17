#include "vec.h"
#include <stdio.h>
#include <stdlib.h>

void vec_realloc_internal(struct vec_internal *vi, size_t new_capacity,
                          size_t esize) {
    if (new_capacity == 0)
        abort();
    size_t new_size = safe_mul(new_capacity, esize);
    if (vi->els == vi->storage) {
        void *new = malloc(new_size);
        size_t min_cap = new_capacity < vi->capacity ? new_capacity : vi->capacity;
        memcpy(new, vi->els, min_cap * esize);
        vi->els = new;
    } else {
        vi->els = realloc(vi->els, new_size);
    }
    vi->capacity = new_capacity;
}
void vec_realloc_internal_as_necessary(struct vec_internal *vi,
                                       size_t min_capacity,
                                       size_t esize) {
    if (min_capacity > vi->capacity)
        vec_realloc_internal(vi, safe_mul(vi->capacity, 2), esize);
    else if (min_capacity < vi->capacity / 3)
        vec_realloc_internal(vi, vi->capacity / 3, esize);
}
