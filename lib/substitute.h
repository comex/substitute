/*
    libsubstitute - https://github.com/comex/substitute
    This header file itself is in the public domain (or in any jusrisdiction
    where the former is ineffective, CC0 1.0).
*/

#pragma once

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Error codes */
enum {
    /* TODO add numbers */
    SUBSTITUTE_OK = 0,

    /* substitute_hook_functions: can't patch a function because it's too short-
     * i.e. there's an unconditional return instruction inside the patch region
     * (and not at its end) */
    SUBSTITUTE_ERR_FUNC_TOO_SHORT,

    /* substitute_hook_functions: can't patch a function because one of the
     * instructions within the patch region is one of a few special problematic
     * cases - if you get this on real code, the library should probably be
     * updated to handle that case properly */
    SUBSTITUTE_ERR_FUNC_BAD_INSN_AT_START,

    /* substitute_hook_functions: can't patch a function because the (somewhat
     * cursory) jump analysis found a jump later in the function to within the
     * patch region at the beginning */
    SUBSTITUTE_ERR_FUNC_JUMPS_TO_START,

    /* mmap or mprotect failure other than ENOMEM (preserved in errno on return
     * from the substitute_* function).  Most likely to come up with
     * substitute_hook_functions, if the kernel is preventing pages from being
     * marked executable. */
    SUBSTITUTE_ERR_VM,

    /* substitute_interpose_imports: couldn't redo relocation for an import
     * because the type was unknown */
    SUBSTITUTE_ERR_UNKNOWN_RELOCATION_TYPE,
};

struct substitute_function_hook {
    void *function;
    void *replacement;
    void *old_ptr; /* optional: out pointer to function pointer to call old impl */
};

/* TODO doc */
int substitute_hook_functions(const struct substitute_function_hook *hooks,
                              size_t nhooks,
                              int options);

#if 1 /* declare dynamic linker-related stuff? */

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
 * @return   a handle, or NULL if the image wasn't found
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
 * @names  an array of symbol names to search for
 * @syms   an array of substitute_sym *, one per name; on return, each entry
 *         will be a pointer into the symbol table for that image, or NULL if
 *         the symbol wasn't found
 * @nsyms  number of names
 *
 * @return SUBSTITUTE_OK (maybe errors in the future)
 */
int substitute_find_private_syms(struct substitute_image *handle,
                                 const char **names,
                                 substitute_sym **syms,
                                 size_t nsyms);

/* Get a pointer corresponding to a loaded symbol table entry.
 * @handle handle containing the symbol
 * @sym    symbol
 * @return the pointer - on ARM, this can be | 1 for Thumb, like everything
 * else
 */
void *substitute_sym_to_ptr(struct substitute_image *handle, substitute_sym *sym);

struct substitute_import_hook {
    /* The symbol name - this is raw, so C++ symbols are mangled, and on OS X
     * most symbols have '_' prepended. */
    const char *name;
    /* The new import address. */
    void *replacement;
    /* Optional: out pointer to old value.  if there are multiple imports for
     * the same symbol, only one address is returned (hopefully they are all
     * equal) */
    void *old_ptr;
};

/* Directly modify the GOT/PLT entries from a specified image corresponding to
 * specified symbols.
 *
 * This can be used to 'hook' functions or even exported variables.  Compared
 * to substitute_hook_functions, it has the following advantages:
 *
 * - Because it does not require the ability to patch executable code;
 *   accordingly, it can (from a technical rather than policy perspective) be
 *   used in sandboxed environments like iOS or PaX MPROTECT.
 * - On platforms without RELRO or similar, it is thread safe, as the patches
 *   are done using atomic instructions.
 * - It does not require architecture specific code.
 * - It can be used to modify a single library's view of the world without
 *   affecting the rest of the program.
 *
 * ...and the following disadvantages:
 *
 * - It only works for exported functions, and even then will not catch calls
 *   from a library to its own exported functions.
 * - At present, it *only* works for a single importing library at a time.
 *   Although it is not difficult on most platforms to iterate loaded libraries
 *   in order to hook all of them, substitute does not currently provide this
 *   functionality, traversing all libraries' symbol tables may be slow, and in
 *   any case there is the matter of new importers being loaded after the fact.
 *
 * @handle   handle of the importing library
 * @hooks    see struct substitute_import_hook
 * @nhooks   number of hooks
 * @options  options - pass 0.
 * @return   SUBSTITUTE_OK
 *           SUBSTITUTE_ERR_UNKNOWN_RELOCATION_TYPE
 *           SUBSTITUTE_ERR_VM - in the future with RELRO on Linux
 */
int substitute_interpose_imports(const struct substitute_image *handle,
                                 const struct substitute_import_hook *hooks,
                                 size_t nhooks, int options);


#endif /* 1 */

#ifdef __cplusplus
} /* extern */
#endif
