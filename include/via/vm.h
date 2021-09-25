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
    VIA_BEGIN_PROC = 0x50,
    VIA_LOOKUP_PROC = 0x60,
    VIA_APPLY_PROC = 0x80,
    VIA_ASSUME_PROC = 0x88,
    VIA_IF_PROC = 0x90
};

enum via_reg {
    VIA_REG_PC,
    VIA_REG_EXPR,
    VIA_REG_ARGS,
    VIA_REG_PROC,
    VIA_REG_ENV,
    VIA_REG_EXCH,
    VIA_REG_SPTR,
    VIA_REG_CTXT,

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
    
    struct via_value** frames;
    size_t frames_size;
    size_t frames_top;

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

struct via_value** via_make_array(struct via_vm* vm, size_t size);

struct via_value* via_make_frame(struct via_vm* vm);

struct via_value* via_make_env(struct via_vm* vm);

struct via_value* via_sym(struct via_vm* vm, const char* name);

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

void via_b_quote(struct via_vm* vm);

void via_b_begin(struct via_vm* vm);

void via_b_yield(struct via_vm* vm);

void via_b_if(struct via_vm* vm);

void via_b_context(struct via_vm* vm);

void via_b_apply(struct via_vm* vm);

void via_b_cons(struct via_vm* vm);

void via_b_car(struct via_vm* vm);

void via_b_cdr(struct via_vm* vm);

void via_b_list(struct via_vm* vm);

void via_b_add(struct via_vm* vm);

void via_b_sub(struct via_vm* vm);

void via_b_mul(struct via_vm* vm);

void via_b_div(struct via_vm* vm);

void via_b_mod(struct via_vm* vm);

void via_b_pow(struct via_vm* vm);

void via_b_sin(struct via_vm* vm);

void via_b_cos(struct via_vm* vm);

void via_env_lookup(struct via_vm* vm);

struct via_value* via_get(struct via_vm* vm, const char* symbol_name);

struct via_value* via_list(struct via_vm* vm, ...);

struct via_value* via_formals(struct via_vm* vm, ...);

void via_push_arg(struct via_vm* vm, struct via_value* val);

struct via_value* via_pop_arg(struct via_vm* vm);

struct via_value* via_context(struct via_vm* vm);

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

void via_garbage_collect(struct via_vm* vm);

struct via_value* via_run(struct via_vm* vm);

#ifdef __cplusplus
}
#endif

