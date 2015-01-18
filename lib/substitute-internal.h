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
    #define TARGET_UNSUPPORTED
#elif defined(FORCE_TARGET_arm)
    #define TARGET_arm
    #define TARGET_SUPPORTED
#elif defined(FORCE_TARGET_arm64)
    #define TARGET_arm64
    #define TARGET_SUPPORTED
#elif defined(__x86_64__)
    #define TARGET_x86_64
#elif defined(__i386__)
    #define TARGET_i386
#elif defined(__arm__)
    #define TARGET_arm
    #define TARGET_SUPPORTED
#elif defined(__arm64__)
    #define TARGET_arm64
    #define TARGET_SUPPORTED
#else
    #error target?
#endif
