#pragma once

#include <via/defs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct via_value;
struct via_vm;

struct via_value* via_except_syntax_error(
    struct via_vm* vm,
    const char* message
);

struct via_value* via_except_out_of_memory(
    struct via_vm* vm,
    const char* message
);

struct via_value* via_except_invalid_type(
    struct via_vm* vm,
    const char* message
);

struct via_value* via_except_argument_error(
    struct via_vm* vm,
    const char* message
);

struct via_value* via_except_undefined_value(
    struct via_vm* vm,
    const char* message
);

struct via_value* via_except_out_of_bounds(
    struct via_vm* vm,
    const char* message
);

#ifdef __cplusplus
}
#endif

