#pragma once

#include <via/defs.h>
#include <via/value.h>

#ifdef __cplusplus
extern "C" {
#endif

struct via_value;

enum via_routines {
    VIA_EVAL_PROC = 0,
    VIA_EVAL_COMPOUND_PROC = 0x20,
    VIA_LOOKUP_PROC = 0x60,
    VIA_APPLY_PROC = 0x80
};

enum via_reg {
    VIA_REG_PC,
    VIA_REG_EXPR,
    VIA_REG_ARGS,
    VIA_REG_PROC,
    VIA_REG_ENV,
    VIA_REG_EXCN,

    VIA_REG_COUNT
};

struct via_segment {
    struct via_value* values;
    size_t num_values;
    size_t count;
    struct via_segment* next;
};

struct via_vm {
    struct via_segment* heap;

    via_int* program;
    size_t write_cursor;
    size_t program_cap;

    void(**bound)(struct via_vm*);
    size_t num_bound;
    size_t bound_cap;

    struct via_value* symbols;

    struct via_value* regs[VIA_REG_COUNT];
    struct via_value* acc;
    struct via_value* ret;

    struct via_value** stack;
    size_t stack_size;
    size_t stack_top;
};

struct via_vm* via_create_vm();

void via_free_vm(struct via_vm* vm);

struct via_value* via_make_value(struct via_vm* vm);

struct via_value* via_make_int(struct via_vm* vm, via_int v_int);
struct via_value* via_make_float(struct via_vm* vm, via_float v_float);
struct via_value* via_make_bool(struct via_vm* vm, via_bool v_bool);
struct via_value* via_make_string(struct via_vm* vm, const char* v_string);
struct via_value* via_make_pair(
    struct via_vm* vm,
    struct via_value* car,
    struct via_value* cdr
);

struct via_value** via_make_array(struct via_vm* vm, size_t size);

struct via_value* via_make_frame(struct via_vm* vm);

struct via_value* via_make_env(struct via_vm* vm);

struct via_value* via_symbol(struct via_vm* vm, const char* name);

void via_assume_frame(struct via_vm* vm);

via_int via_bind(struct via_vm* vm, void(*func)(struct via_vm*));

void via_register_proc(
    struct via_vm* vm,
    const char* symbol,
    struct via_value* formals,
    void(*func)(struct via_vm*)
);

void via_register_form(
    struct via_vm* vm,
    const char* symbol,
    void(*func)(struct via_vm*)
);

void via_apply(struct via_vm* vm);

void via_form(struct via_vm* vm);

void via_env_lookup(struct via_vm* vm);

void via_env_set(
    struct via_vm* vm,
    struct via_value* symbol,
    struct via_value* value
);

struct via_value* via_get(struct via_vm* vm, const char* symbol_name);

void via_lookup_form(struct via_vm* vm);

void via_set_form(
    struct via_vm* vm,
    struct via_value* symbol,
    struct via_value* definition
);

struct via_value* via_run(struct via_vm* vm);

#ifdef __cplusplus
}
#endif

