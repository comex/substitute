#pragma once

#include <stdio.h>
#define panic(...) do { \
	fprintf(stderr, __VA_ARGS__); \
	abort(); \
	__builtin_unreachable(); \
} while(0)

#define EXPORT __attribute__ ((visibility("default")))
