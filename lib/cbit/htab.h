#pragma once
#include "misc.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct htab_internal {
    size_t length;
    size_t capacity;
    void *base;
    char hi_storage[1]; // see vec.h
};

#define UNUSED_STATIC_INLINE __attribute__((unused)) static inline

/* Declare the helper functions for a htab key type without static - put this
 * in the .h and IMPL_HTAB_KEY in a .c. */
#define DECL_EXTERN_HTAB_KEY( \
    name, \
    /* The key type. */ \
    key_ty \
) \
    DO_DECL_HTAB_KEY(name, key_ty, )

/* Declare and implement the helper functions static inline.  For the argument
 * meanings, see IMPL_HTAB_KEY. */
#define DECL_STATIC_HTAB_KEY( \
    name, \
    key_ty, \
    hash_func, \
    eq_func, \
    null_func, \
    nil_byte \
) \
    DO_DECL_HTAB_KEY(name, key_ty, UNUSED_STATIC_INLINE); \
    IMPL_HTAB_KEY(name, hash_func, eq_func, null_func, nil_byte)

#define DO_DECL_HTAB_KEY(name, key_ty, func_decl) \
    typedef key_ty __htab_key_key_ty_##name; \
    DO_DECL_HTAB_KEY_(name, __htab_key_key_ty_##name, func_decl)
#define DO_DECL_HTAB_KEY_(name, key_ty, func_decl) \
    func_decl \
    void *__htab_key_lookup_##name(struct htab_internal *restrict hi, \
                                   const key_ty *restrict key, \
                                   size_t entry_size, \
                                   bool add, bool resize_if_necessary); \
    func_decl \
    bool __htab_key_remove_##name(struct htab_internal *restrict hi, \
                                  const key_ty *restrict key, \
                                  size_t entry_size); \
    func_decl \
    void __htab_key_memset_##name(void *ptr, size_t size); \
    func_decl \
    bool __htab_key_is_null_##name(const key_ty *bucket); \
    func_decl \
    void __htab_key_resize_##name(struct htab_internal *restrict hi, \
                                  size_t size, \
                                  size_t entry_size) \

#define IMPL_HTAB_KEY(name, \
    /* size_t hash_func(const ty *) - hash. \
     * May be a macro. */ \
    hash_func, \
    /* bool eq_func(const ty *stored, const ty *queried) - check whether the \
     * stored key is equal to the requested key (which is assumed to be valid). \
     * May be a macro. */ \
    eq_func, \
    /* bool null_func(const ty *) - check whether the stored key is a \
     * special invalid marker. \
     * May be a macro. */ \
    null_func, \
    /* uint8_t nil_byte - which byte to memset to initially. \
     * null_func(<all bytes nil_byte>) must be true. */ \
    nil_byte \
) \
    DO_IMPL_HTAB_KEY(name, __htab_key_key_ty_##name, hash_func, \
                     eq_func, null_func, nil_byte)
#define DO_IMPL_HTAB_KEY(name, key_ty, hash_func, eq_func, null_func, nil_byte) \
    void __htab_key_resize_##name(struct htab_internal *restrict hi, \
                                  size_t capacity, \
                                  size_t entry_size); \
    void *__htab_key_lookup_##name(struct htab_internal *restrict hi, \
                                   const key_ty *restrict key, \
                                   size_t entry_size, \
                                   bool add, bool resize_if_necessary) { \
        if (resize_if_necessary && \
            hi->capacity * 2 <= hi->length * 3) \
            __htab_key_resize_##name(hi, hi->capacity * 2, entry_size); \
        size_t capacity = hi->capacity; \
        size_t hash = (hash_func(key)) % capacity; \
        size_t i = hash; \
        char *buckets = hi->base; \
        do { \
            key_ty *bucket = (void *) (buckets + i * entry_size); \
            if (null_func(bucket)) { \
                if (add) { \
                    *bucket = *key; \
                    hi->length++; \
                    return bucket; \
                } else { \
                    return NULL; \
                } \
            } \
            if (eq_func(bucket, key)) \
                return bucket; \
        } while ((i = (i + 1) % capacity) != hash); \
        return NULL; \
    } \
    \
    /* slow but who cares */ \
    bool __htab_key_remove_##name(struct htab_internal *restrict hi, \
                                  const key_ty *restrict key, \
                                  size_t entry_size) { \
        void *op = __htab_key_lookup_##name(hi, key, entry_size, false, false); \
        if (!op) \
            return false; \
        key_ty *orig = op; \
        key_ty *end = (void *) ((char *) hi->base + hi->capacity * entry_size); \
        key_ty *cur = orig; \
        key_ty *prev; \
        do { \
            prev = cur; \
            cur = (void *) ((char *) cur + entry_size); \
            if (cur == end) \
                cur = hi->base; \
            if (null_func(cur)) \
                break; \
            memmove(prev, cur, entry_size); \
            prev = cur; \
        } while (cur != orig); \
        memset(cur, 0, entry_size); \
        hi->length--; \
        return true; \
    } \
    void __htab_key_memset_##name(void *ptr, size_t size) { \
        memset(ptr, (nil_byte), size); \
    } \
    bool __htab_key_is_null_##name(const key_ty *bucket) { \
        return null_func(bucket); \
    } \
    void __htab_key_resize_##name(struct htab_internal *restrict hi, \
                                  size_t size, \
                                  size_t entry_size) { \
        size_t old_size = hi->capacity * entry_size; \
        size_t new_size = safe_mul(size, entry_size); \
        void *new_buf = malloc(new_size); \
        memset(new_buf, (nil_byte), new_size); \
        struct htab_internal temp; \
        temp.length = 0; \
        temp.capacity = size; \
        temp.base = new_buf; \
        for (size_t i = 0; i < old_size; i += entry_size) { \
            key_ty *bucket = (void *) ((char *) hi->base + i); \
            if (!null_func(bucket)) { \
                memcpy( \
                    __htab_key_lookup_##name(&temp, bucket, entry_size, \
                                             true, false), \
                    bucket, \
                    entry_size); \
            } \
        } \
        hi->capacity = size; \
        if (hi->base != hi->hi_storage) \
            free(hi->base); \
        hi->base = new_buf; \
    } \
    typedef char __htab_want_semicolon_here_##name

#define DECL_HTAB( \
    name, \
    /* The name parameter to DECL_HTAB_KEY */ \
    key_name, \
    value_ty) \
    typedef __htab_key_key_ty_##key_name __htab_key_ty_##name; \
    typedef value_ty __htab_value_ty_##name; \
    \
    DO_DECL_HTAB(name, \
                 __htab_key_ty_##name, \
                 __htab_value_ty_##name, \
                 struct htab_bucket_##name, \
                 struct htab_##name, \
                 key_name)

#define DO_DECL_HTAB(name, key_ty, value_ty, bucket_ty, htab_ty, key_name) \
    bucket_ty { \
        key_ty key; \
        value_ty value; \
    }; \
    htab_ty { \
        union { \
            char h[0]; \
            struct htab_internal hi; \
            struct { \
                size_t length; \
                size_t capacity; \
                bucket_ty *base; \
                bucket_ty storage[1]; \
            }; \
        }; \
    }; \
    UNUSED_STATIC_INLINE \
    bucket_ty *htab_getbucket_##name(htab_ty *restrict ht, \
                                     const key_ty *restrict key) { \
        return __htab_key_lookup_##key_name(&ht->hi, key, sizeof(bucket_ty), \
                                            false, true); \
    } \
    UNUSED_STATIC_INLINE \
    value_ty *htab_getp_##name(const htab_ty *restrict ht, \
                               const key_ty *restrict key) { \
        bucket_ty *bucket = htab_getbucket_##name((void *) ht, key); \
        return bucket ? &bucket->value : NULL; \
    } \
    UNUSED_STATIC_INLINE \
    bucket_ty *htab_setbucket_##name(htab_ty *restrict ht, \
                                                const key_ty *restrict key) { \
        return __htab_key_lookup_##key_name(&ht->hi, key, sizeof(bucket_ty), \
                                            true, true); \
    } \
    UNUSED_STATIC_INLINE \
    value_ty *htab_setp_##name(const htab_ty *restrict ht, \
                               const key_ty *restrict key) { \
        bucket_ty *bucket = htab_setbucket_##name((void *) ht, key); \
        return bucket ? &bucket->value : NULL; \
    } \
    UNUSED_STATIC_INLINE \
    bool htab_remove_##name(htab_ty *restrict ht, const key_ty *restrict key) { \
        return __htab_key_remove_##key_name(&ht->hi, key, sizeof(bucket_ty)); \
    } \
    UNUSED_STATIC_INLINE \
    void __htab_memset_##name(void *ptr, size_t size) { \
        return __htab_key_memset_##key_name(ptr, size); \
    } \
    UNUSED_STATIC_INLINE \
    bool __htab_is_null_##name(const key_ty *bucket) { \
        return __htab_key_is_null_##key_name(bucket); \
    } \
    UNUSED_STATIC_INLINE \
    void htab_resize_##name(htab_ty *ht, size_t size) { \
        return __htab_key_resize_##key_name(&ht->hi, size, sizeof(bucket_ty)); \
    } \
    UNUSED_STATIC_INLINE \
    void htab_free_##name(htab_ty *ht) { \
        if (ht->base != ht->storage) \
            free(ht->base); \
    }

#define HTAB_STORAGE_CAPA(name, n) \
   struct { \
      struct htab_##name h; \
      struct htab_bucket_##name rest[(n)-1]; \
   }

#define HTAB_STORAGE(name) \
    HTAB_STORAGE_CAPA(name, 5)

#define HTAB_STORAGE_INIT(hs, name) do { \
    struct htab_##name *h = &(hs)->h; \
    h->length = 0; \
    h->capacity = (sizeof((hs)->rest) / sizeof(__htab_key_ty_##name)) + 1; \
    h->base = h->storage; \
    __htab_memset_##name(h->base, \
                         h->capacity * sizeof(__htab_key_ty_##name)); \
} while (0)

#define HTAB_FOREACH(ht, key_var, val_var, name) \
    LET(struct htab_##name *__ht = (ht)) \
        for (size_t __htfe_bucket = 0; __htfe_bucket < __ht->capacity; __htfe_bucket++) \
            if(__htab_is_null_##name(&__ht->base[__htfe_bucket].key)) \
                continue; \
            else \
                LET_LOOP(key_var = &__ht->base[__htfe_bucket].key) \
                LET_LOOP(val_var = &__ht->base[__htfe_bucket].value)

