/*
    libsubstitute - https://github.com/comex/substitute
    This header file is in the public domain (or in any jusrisdiction where the
    former is ineffective, CC0 1.0).
*/

#pragma once

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TODO add numbers */
enum {
    SUBSTITUTE_OK = 0,
};

int substitute_hook_function(void *function, void *replacement, int options, void *result);

#if 1 /* declare substitute_find_syms? */

#ifdef __APPLE__
#include <mach-o/nlist.h>
#ifdef __LP64__
typedef struct nlist_64 substitute_sym;
#else
typedef struct nlist substitute_sym;
#endif
#else
#error No definition for substitute_sym!
#endif

struct substitute_image {
#ifdef __APPLE__
    intptr_t slide;
    void *dlhandle;
    const void *image_header;
#endif
    /* possibly private fields... */
};

/* Look up an image currently loaded into the process.
 *
 * @filename the executable/library path (c.f. dyld(3) on Darwin)
 * @return a handle, or NULL if the image wasn't found
 */
struct substitute_image *substitute_open_image(const char *filename);

/* Release a handle opened with substitute_open_image.
 *
 * @handle a banana
 */
void substitute_close_image(struct substitute_image *handle);

/* Look up private symbols in an image currently loaded into the process.
 *
 * @handle handle opened with substitute_open_image
 * @names an array of symbol names to search for
 * @nlist an array of substitute_sym *, one per name; on return, each entry
 * will be a pointer into the symbol table for that image, or NULL if the
 * symbol wasn't found
 * @count number of names
 *
 * @return SUBSTITUTE_OK (maybe errors in the future)
 */
int substitute_find_private_syms(struct substitute_image *handle,
                                 const char **names,
                                 substitute_sym **syms,
                                 size_t count);

/* Get a pointer corresponding to a loaded symbol table entry.
 * @handle handle containing the symbol
 * @sym symbol
 * @return the pointer - on ARM, this can be | 1 for Thumb, like everything
 * else
 */
void *substitute_sym_to_ptr(struct substitute_image *handle, substitute_sym *sym);

#endif /* 1 */

#ifdef __cplusplus
} /* extern */
#endif
