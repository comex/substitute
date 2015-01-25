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

    /* out of memory */
    SUBSTITUTE_ERR_OOM,

    /* substitute_hook_functions:    mmap or mprotect failure other than ENOMEM
     *                               (preserved in errno on return)
     * substitute_hook_functions:    vm_region failure (errno = 0)
     * substitute_hook_objc_message: vm_remap failure
     * Most likely to come up with substitute_hook_functions if the kernel is
     * preventing pages from being marked executable. */
    SUBSTITUTE_ERR_VM,

    /* substitute_hook_functions: not on the main thread (so stopping all other
     * threads would be unsafe, as concurrent attempts to do the same from
     * other threads would result in deadlock), and you did not pass
     * SUBSTITUTE_DONT_STOP_THREADS */
    SUBSTITUTE_ERR_NOT_ON_MAIN_THREAD,

    /* substitute_interpose_imports: couldn't redo relocation for an import
     * because the type was unknown */
    SUBSTITUTE_ERR_UNKNOWN_RELOCATION_TYPE,

    /* substitute_hook_objc_message: no such selector existed in the class's
     * inheritance tree */
    SUBSTITUTE_ERR_NO_SUCH_SELECTOR,

    /* substitute_hook_functions: OS error suspending other threads */
    SUBSTITUTE_ERR_ADJUSTING_THREADS,
};

struct substitute_function_hook {
    void *function;
    void *replacement;
    void *old_ptr; /* optional: out pointer to function pointer to call old impl */
};

/* Get a string representation for a SUBSTITUTE_* error code. */
const char *substitute_strerror(int err);

/* substitute_hook_functions options */
enum {
    SUBSTITUTE_DONT_STOP_THREADS = 1,
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
 * @syms   an array of void *, one per name; on return, each entry will be
 *         filled in with the corresponding symbol address, or NULL if the
 *         symbol wasn't found
 *         (on ARM, this will be | 1 for Thumb functions)
 * @nsyms  number of names
 *
 * @return SUBSTITUTE_OK (maybe errors in the future)
 */
int substitute_find_private_syms(struct substitute_image *handle,
                                 const char **__restrict names,
                                 void **__restrict syms,
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

#if defined(__APPLE__)
#include <objc/runtime.h>
/* Hook a method implementation for a given Objective-C class.  By itself, this
 * function is thread safe: it is simply a wrapper for the atomic Objective-C
 * runtime call class_replaceMethod, plus the superclass-call generation
 * functionality described below, and some code to ensure atomicity if the
 * method is called while the function is in progress.  However, it will race
 * with code that modifies class methods without using atomic runtime calls,
 * such as Substrate.
 *
 * @klass            the class
 * @selector         the selector
 * @replacement      the new implementation (other APIs would call this an
 *                   IMP, but that isn't in general the real type of the
 *                   implementation, so declared as a void * here)
 * @old_ptr          optional - out pointer to the 'old implementation'.
 *                   If there is no old implementation, a custom IMP is
 *                   returned that delegates to the superclass.  This IMP can
 *                   be freed if desired with imp_removeBlock.
 * @created_imp_ptr  optional - out pointer to whether a fake superclass-call
 *                   IMP has been placed in <old_ptr>
 *
 * @return           SUBSTITUTE_OK
 *                   SUBSTITUTE_ERR_NO_SUCH_SELECTOR
 */
int substitute_hook_objc_message(Class klass, SEL selector, void *replacement,
                                 void *old_ptr, bool *created_imp_ptr);

void substitute_free_created_imp(IMP imp);
#endif

#ifdef __cplusplus
} /* extern */
#endif
