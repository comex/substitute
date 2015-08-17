#pragma once
#include "misc.h"

struct cqueue_internal {
    char *start, *end, *read_ptr, *write_ptr;
};

#ifdef __cplusplus
extern "C" {
#endif

void cqueue_realloc_internal(struct cqueue_internal *ci,
                             size_t new_capacity_bytes);
#ifdef __cplusplus
}
#endif

#define DECL_CQUEUE(ty, name) \
    typedef ty __CQUEUE_TY_##name; \
    struct cqueue_##name { \
        union { \
            struct cqueue_internal ci; \
            struct { \
                __CQUEUE_TY_##name *start, *end, *read_ptr, *write_ptr; \
            }; \
        }; \
    }; \
    UNUSED_STATIC_INLINE \
    void cqueue_free_storage_##name(struct cqueue_##name *c) { \
        free(c->start); \
    } \
    UNUSED_STATIC_INLINE \
    void cqueue_realloc_##name(struct cqueue_##name *c, size_t new_capacity) { \
        cqueue_realloc_internal(&c->ci, safe_mul(new_capacity, \
                                                 sizeof(__CQUEUE_TY_##name))); \
    } \
    UNUSED_STATIC_INLINE \
    __CQUEUE_TY_##name *cqueue_appendp_##name(struct cqueue_##name *c) { \
        if (c->write_ptr + 1 == c->read_ptr) \
            cqueue_realloc_##name(c, (c->end - c->start) * 2 + 3); \
        __CQUEUE_TY_##name *ret = c->write_ptr++; \
        if (c->write_ptr == c->end) \
            c->write_ptr = c->start; \
        return ret; \
    } \
    UNUSED_STATIC_INLINE \
    __CQUEUE_TY_##name *cqueue_shiftp_##name(struct cqueue_##name *c) { \
        if (c->read_ptr == c->write_ptr) \
            return NULL; \
        __CQUEUE_TY_##name *ret = c->read_ptr++; \
        if (c->read_ptr == c->end) \
            c->read_ptr = c->start; \
        return ret; \
    } \
    UNUSED_STATIC_INLINE \
    size_t cqueue_length_##name(const struct cqueue_name *c) { \
        return ((c->write_ptr - c->start) - (c->read_ptr - c->start)) \
            % (c->end - c->start); \
    } \
    typedef char __plz_end_decl_cqueue_with_semicolon_##name
