#pragma once
#include "misc.h"
#include <stdlib.h>
#include <string.h>

struct vec_internal {
   size_t length;
   size_t capacity;
   void *els;
   char storage[1]; // must be at least one byte so vec_free works correctly
};

#ifdef __cplusplus
extern "C" {
#endif

void vec_realloc_internal(struct vec_internal *vi, size_t new_capacity,
                          size_t esize);
void vec_realloc_internal_as_necessary(struct vec_internal *vi,
                                       size_t min_capacity,
                                       size_t esize);
#ifdef __cplusplus
}
#endif

#define DECL_VEC(ty, name) \
   typedef ty __VEC_TY_##name; \
   struct vec_##name { \
      union { \
         struct vec_internal vi; \
         struct { \
            size_t length; \
            size_t capacity; \
            VEC_TY(name) *els; \
            VEC_TY(name) storage[1]; \
         }; \
      }; \
   }; \
   UNUSED_STATIC_INLINE \
   void vec_free_storage(struct vec_##name *v) { \
      if (v->els != v->storage) \
         free(v->els); \
   } \
   UNUSED_STATIC_INLINE \
   struct vec_##name vec_borrow(VEC_TY(name) *els, size_t length) { \
      struct vec_##name v; \
      v.length = v.capacity = length; \
      v.els = els; \
      return v; \
   } \
   UNUSED_STATIC_INLINE \
   void vec_realloc_##name(struct vec_##name *v, size_t new_capacity) { \
      vec_realloc_internal(&v->vi, new_capacity, sizeof(v->els[0])); \
   } \
   UNUSED_STATIC_INLINE \
   void vec_resize_##name(struct vec_##name *v, size_t new_length) { \
      if (new_length >= v->capacity || new_length < v->capacity / 3) \
         vec_realloc_internal_as_necessary(&v->vi, new_length, sizeof(v->els[0])); \
      v->length = new_length; \
   } \
   UNUSED_STATIC_INLINE \
   VEC_TY(name) *vec_appendp_##name(struct vec_##name *v) { \
      size_t i = v->length++; \
      if (i == v->capacity) \
         vec_realloc_internal_as_necessary(&v->vi, i + 1, sizeof(v->els[0])); \
      return &v->els[i]; \
   } \
   UNUSED_STATIC_INLINE \
   void vec_append_##name(struct vec_##name *v, VEC_TY(name) val) { \
      *vec_appendp_##name(v) = val; \
   } \
   UNUSED_STATIC_INLINE \
   VEC_TY(name) vec_pop_##name(struct vec_##name *v) { \
      size_t i = v->length - 1; \
      VEC_TY(name) ret = v->els[i]; \
      if (v->els != v->storage && i < v->capacity / 3) \
         vec_realloc_internal_as_necessary(&v->vi, i, sizeof(v->els[0])); \
      v->length = i; \
      return ret; \
   } \
   UNUSED_STATIC_INLINE \
   void vec_concat_##name(struct vec_##name *v1, const struct vec_##name *v2) { \
      size_t l1 = v1->length, l2 = v2->length; \
      vec_resize_##name(v1, safe_add(l1, l2)); \
      memcpy(&v1->els[l1], v2->els, l2 * sizeof(v1->els[0])); \
   } \
   UNUSED_STATIC_INLINE \
   void vec_add_space_##name(struct vec_##name *v, size_t idx, size_t num) { \
      /* todo optimize */ \
      size_t orig = v->length; \
      vec_resize_##name(v, orig + num); \
      memmove(&v->els[idx + num], &v->els[idx], \
              (orig - idx) * sizeof(v->els[0])); \
   } \
   UNUSED_STATIC_INLINE \
   void vec_remove_##name(struct vec_##name *v, size_t idx, size_t num) { \
      /* todo optimize */ \
      size_t orig = v->length; \
      memmove(&v->els[idx], &v->els[idx + num], \
              (orig - (idx + num)) * sizeof(v->els[0])); \
      vec_resize_##name(v, orig - num); \
   } \


#define VEC_TY(name) __VEC_TY_##name

#define VEC_STORAGE_CAPA(name, n) \
   struct { \
      struct vec_##name v; \
      VEC_TY(name) rest[(n)-1]; \
   }

#define VEC_STORAGE(ty) \
   VEC_STORAGE_CAPA(ty, 5)

#define VEC_STORAGE_INIT(vs, name) do { \
   struct vec_##name *v = &(vs)->v; \
   v->length = 0; \
   v->capacity = (sizeof((vs)->rest) / sizeof(VEC_TY(name))) + 1; \
   v->els = v->storage; \
} while (0)


/* guaranteed to *not* cache vec->length - pretty simple */

#define VEC_FOREACH(vec, idx_var, ptr_var, name) \
   LET(struct vec_##name *__vfe_v = (vec)) \
      for (size_t idx_var = 0; idx_var < __vfe_v->length; idx_var++) \
         LET_LOOP(ptr_var = &__vfe_v->els[idx_var])

