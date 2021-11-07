#pragma once

#include <via/defs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct via_port_handle;
struct via_value;
struct via_vm;

const struct via_value* via_make_int(struct via_vm* vm, via_int v_int);
const struct via_value* via_make_float(struct via_vm* vm, via_float v_float);
const struct via_value* via_make_bool(struct via_vm* vm, via_bool v_bool);

const struct via_value* via_make_string(
    struct via_vm* vm,
    const char* v_string
);

const struct via_value* via_make_stringview(
    struct via_vm* vm,
    const char* v_string
);

const struct via_value* via_make_builtin(
    struct via_vm* vm,
    via_int v_builtin
);

const struct via_value* via_make_pair(
    struct via_vm* vm,
    const struct via_value* car,
    const struct via_value* cdr
);

const struct via_value* via_make_proc(
    struct via_vm* vm,
    const struct via_value* body,
    const struct via_value* formals,
    const struct via_value* env
);

const struct via_value* via_make_form(
    struct via_vm* vm,
    const struct via_value* formals,
    const struct via_value* body,
    const struct via_value* routine
);

const struct via_value* via_make_handle(struct via_vm* vm, void* handle);

const struct via_value** via_make_array(struct via_vm* vm, size_t size);

const struct via_value* via_make_env(
    struct via_vm* vm,
    const struct via_value* parent
);

const struct via_value* via_escape_string(
    struct via_vm* vm,
    const struct via_value* value
);

const struct via_value* via_to_string(
    struct via_vm* vm,
    const struct via_value* value
);

const struct via_value* via_to_string_raw(
    struct via_vm* vm,
    const struct via_value* value
);

const struct via_value* via_string_concat(
    struct via_vm* vm,
    const struct via_value* a,
    const struct via_value* b
);

const struct via_value* via_list(struct via_vm* vm, ...);

const struct via_value* via_formals(struct via_vm* vm, ...);

const struct via_value* via_reverse_list(
    struct via_vm* vm,
    const struct via_value* list
);

#ifdef __cplusplus
}
#endif

