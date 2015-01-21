#pragma once

#include <stdio.h>
#define substitute_panic(...) do { \
    fprintf(stderr, __VA_ARGS__); \
    abort(); \
    __builtin_unreachable(); \
} while(0)

#define EXPORT __attribute__ ((visibility("default")))
#define UNUSED __attribute__((unused))

#ifdef __APPLE__
#include <mach-o/loader.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#ifdef __LP64__
typedef struct mach_header_64 mach_header_x;
typedef struct segment_command_64 segment_command_x;
typedef struct section_64 section_x;
#define LC_SEGMENT_X LC_SEGMENT_64
#else
typedef struct mach_header mach_header_x;
typedef struct segment_command segment_command_x;
typedef struct section section_x;
#define LC_SEGMENT_X LC_SEGMENT
#endif
#endif

/* FORCE_* are for tests */
#if defined(FORCE_TARGET_x86_64)
    #define TARGET_x86_64
#elif defined(FORCE_TARGET_i386)
    #define TARGET_i386
#elif defined(FORCE_TARGET_arm)
    #define TARGET_arm
#elif defined(FORCE_TARGET_arm64)
    #define TARGET_arm64
#elif defined(__x86_64__)
    #define TARGET_x86_64
#elif defined(__i386__)
    #define TARGET_i386
#elif defined(__arm__)
    #define TARGET_arm
#elif defined(__arm64__)
    #define TARGET_arm64
#else
    #error target?
#endif

#if defined(TARGET_arm)
    #include "arm/misc.h"
#elif defined(TARGET_arm64)
    #include "arm64/misc.h"
#endif

#ifdef __APPLE__
/* This could graduate to a public API but is not yet. */
enum {
    SUBSTITUTE_DIP_INJECT_MAIN_THREAD, /* not yet */
};

enum {
    /* substitute_dlopen_in_pid: task_for_pid failed; on OS X the reasons this
     * can happen are really complicated and dumb, but generally one solution
     * is to be root */
    SUBSTITUTE_ERR_TASK_FOR_PID = 1000,

    /* substitute_dlopen_in_pid: something didn't work */
    SUBSTITUTE_ERR_MISC,
};

int substitute_dlopen_in_pid(int pid, const char *filename, int options, char **error);
#endif
