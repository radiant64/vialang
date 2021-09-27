#pragma once

#include <via/defs.h>
#include <via/value.h>

#ifdef __cplusplus
extern "C" {
#endif

struct via_value;

enum via_routines {
    VIA_EVAL_PROC = 0x01,
    VIA_EVAL_COMPOUND_PROC = 0x20,
    VIA_BEGIN_PROC = 0x50,
    VIA_LOOKUP_PROC = 0x60,
    VIA_APPLY_PROC = 0x80,
    VIA_EXH_PROC = 0x84,
    VIA_ASSUME_PROC = 0x88,
    VIA_IF_PROC = 0x90,
    VIA_SET_PROC = 0xa8,
    VIA_SET_ENV_PROC = 0xc0,
};

enum via_reg {
    VIA_REG_PC,
    VIA_REG_EXPR,
    VIA_REG_ARGS,
    VIA_REG_PROC,
    VIA_REG_ENV,
    VIA_REG_EXCN,
    VIA_REG_EXH,
    VIA_REG_SPTR,
    VIA_REG_CTXT,
    VIA_REG_PARN,

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

    struct via_value* regs;
    struct via_value* acc;
    struct via_value* ret;

    struct via_value** stack;
    size_t stack_size;
    
    uint8_t generation;
};

struct via_vm* via_create_vm();

void via_free_vm(struct via_vm* vm);

struct via_value* via_make_value(struct via_vm* vm);

struct via_value* via_make_int(struct via_vm* vm, via_int v_int);
struct via_value* via_make_float(struct via_vm* vm, via_float v_float);
struct via_value* via_make_bool(struct via_vm* vm, via_bool v_bool);
struct via_value* via_make_string(struct via_vm* vm, const char* v_string);
struct via_value* via_make_stringview(struct via_vm* vm, const char* v_string);
struct via_value* via_make_pair(
    struct via_vm* vm,
    struct via_value* car,
    struct via_value* cdr
);

struct via_value* via_make_proc(
    struct via_vm* vm,
    struct via_value* body,
    struct via_value* formals,
    struct via_value* env
);

struct via_value** via_make_array(struct via_vm* vm, size_t size);

struct via_value* via_make_frame(struct via_vm* vm);

struct via_value* via_make_env(struct via_vm* vm);

struct via_value* via_sym(struct via_vm* vm, const char* name);

void via_return_outer(struct via_vm* vm, struct via_value* value);

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

void via_env_lookup(struct via_vm* vm);

struct via_value* via_get(struct via_vm* vm, const char* symbol_name);

struct via_value* via_list(struct via_vm* vm, ...);

struct via_value* via_formals(struct via_vm* vm, ...);

struct via_value* via_to_string(struct via_vm* vm, struct via_value* value);

void via_push_arg(struct via_vm* vm, struct via_value* val);

struct via_value* via_pop_arg(struct via_vm* vm);

void via_apply(struct via_vm* vm);

struct via_value* via_context(struct via_vm* vm);

struct via_value* via_exception(struct via_vm* vm);

void via_lookup_form(struct via_vm* vm);

void via_set_form(
    struct via_vm* vm,
    struct via_value* symbol,
    struct via_value* definition
);

void via_env_set(
    struct via_vm* vm,
    struct via_value* symbol,
    struct via_value* value
);

void via_b_env_set(struct via_vm* vm);

void via_garbage_collect(struct via_vm* vm);

void via_catch(
    struct via_vm* vm,
    struct via_value* predicate,
    struct via_value* handler
);

void via_throw(struct via_vm* vm, struct via_value* exception);

void via_default_exception_handler(struct via_vm* vm);

struct via_value* via_run(struct via_vm* vm);

#ifdef __cplusplus
}
#endif

