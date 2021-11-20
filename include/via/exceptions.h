#pragma once

#include <via/defs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct via_value;
struct via_vm;

via_bool via_is_exception(struct via_vm* vm, const struct via_value* value);

const struct via_value* via_make_exception(
    struct via_vm* vm,
    const char* symbol,
    const char* message
);

const struct via_value* via_excn_symbol(const struct via_value* exception);

const struct via_value* via_excn_message(const struct via_value* exception);

const struct via_value* via_excn_frame(const struct via_value* exception);

const struct via_value* via_except_syntax_error(
    struct via_vm* vm,
    const char* message
);

const struct via_value* via_except_out_of_memory(
    struct via_vm* vm,
    const char* message
);

const struct via_value* via_except_invalid_type(
    struct via_vm* vm,
    const char* message
);

const struct via_value* via_except_argument_error(
    struct via_vm* vm,
    const char* message
);

const struct via_value* via_except_undefined_value(
    struct via_vm* vm,
    const char* message
);

const struct via_value* via_except_out_of_bounds(
    struct via_vm* vm,
    const char* message
);

const struct via_value* via_except_runtime_error(
    struct via_vm* vm,
    const char* message
);

const struct via_value* via_except_io_error(
    struct via_vm* vm,
    const char* message
);

const struct via_value* via_except_end_of_file(
    struct via_vm* vm,
    const char* message
);

const struct via_value* via_except_no_capability(
    struct via_vm* vm,
    const char* message
);

#ifdef __cplusplus
}
#endif

