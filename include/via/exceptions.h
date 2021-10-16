#pragma once

#include <via/defs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct via_value;
struct via_vm;

via_bool via_is_exception(struct via_vm* vm, struct via_value* value);

struct via_value* via_make_exception(
    struct via_vm* vm,
    const char* symbol,
    const char* message
);

struct via_value* via_excn_symbol(struct via_value* exception);

struct via_value* via_excn_message(struct via_value* exception);

struct via_value* via_excn_frame(struct via_value* exception);

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

struct via_value* via_except_runtime_error(
    struct via_vm* vm,
    const char* message
);

struct via_value* via_except_io_error(
    struct via_vm* vm,
    const char* message
);

struct via_value* via_except_end_of_file(
    struct via_vm* vm,
    const char* message
);

#ifdef __cplusplus
}
#endif

