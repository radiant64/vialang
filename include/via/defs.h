#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef bool via_bool;
typedef int64_t via_int;
typedef double via_float;
typedef char via_char;

#define VIA_FMTId PRId64
#define VIA_FMTIx PRIx64

#ifdef __cplusplus
}
#endif

