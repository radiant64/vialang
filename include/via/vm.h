#pragma once

#include <via/defs.h>
#include <via/value.h>

#ifdef __cplusplus
extern "C" {
#endif

struct via_value;

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

struct via_vm;
typedef void(*via_bindable)(void* user_data);

struct via_vm {
    const struct via_value** heap;
    via_int heap_top;
    via_int heap_free;
    via_int heap_cap;

    via_opcode* program;
    via_int write_cursor;
    size_t program_cap;
    
    char** labels;
    via_int* label_addrs;
    size_t labels_count;
    size_t labels_cap;

    via_bindable* bound;
    void** bound_data;
    size_t bound_count;
    size_t bound_cap;

    struct via_value* symbols;

    struct via_value* regs;
    const struct via_value* acc;
    const struct via_value* ret;

    const struct via_value** stack;
    size_t stack_size;
    
    uint8_t generation;
};

struct via_vm* via_create_vm();

void via_free_vm(struct via_vm* vm);

struct via_value* via_make_value(struct via_vm* vm);

const struct via_value* via_make_frame(struct via_vm* vm);

const struct via_value* via_sym(struct via_vm* vm, const char* name);

void via_return_outer(struct via_vm* vm, const struct via_value* value);

void via_assume_frame(struct via_vm* vm);

via_int via_bind(struct via_vm* vm, const char* name, via_bindable func);

via_int via_bind_context(
    struct via_vm* vm,
    const char* name,
    via_bindable func,
    void* user_data
);

void via_register_proc(
    struct via_vm* vm,
    const char* symbol,
    const char* asm_label,
    const struct via_value* formals,
    via_bindable func
);

void via_register_proc_context(
    struct via_vm* vm,
    const char* symbol,
    const char* asm_label,
    const struct via_value* formals,
    via_bindable func,
    void* user_data
);

void via_register_native_proc(
    struct via_vm* vm,
    const char* symbol,
    const char* asm_label,
    const struct via_value* formals
);

void via_register_form(
    struct via_vm* vm,
    const char* symbol,
    const char* asm_label,
    const struct via_value* formals,
    via_bindable func
);

void via_register_form_context(
    struct via_vm* vm,
    const char* symbol,
    const char* asm_label,
    const struct via_value* formals,
    via_bindable func,
    void* user_data
);

void via_register_native_form(
    struct via_vm* vm,
    const char* symbol,
    const char* asm_label,
    const struct via_value* formals
);

const struct via_value* via_env_lookup_nothrow(
    struct via_vm* vm,
    const struct via_value* symbol
);

void via_env_lookup(struct via_vm* vm);

const struct via_value* via_get(struct via_vm* vm, const char* symbol_name);

void via_push(struct via_vm* vm, const struct via_value* value);

const struct via_value* via_pop(struct via_vm* vm);

void via_push_arg(struct via_vm* vm, const struct via_value* val);

const struct via_value* via_pop_arg(struct via_vm* vm);

void via_apply(struct via_vm* vm);

void via_expand_form(struct via_vm* vm);

struct via_value* via_reg_pc(struct via_vm* vm);
const struct via_value* via_reg_expr(struct via_vm* vm);
const struct via_value* via_reg_proc(struct via_vm* vm);
const struct via_value* via_reg_args(struct via_vm* vm);
const struct via_value* via_reg_env(struct via_vm* vm);
const struct via_value* via_reg_excn(struct via_vm* vm);
const struct via_value* via_reg_exh(struct via_vm* vm);
struct via_value* via_reg_sptr(struct via_vm* vm);
const struct via_value* via_reg_ctxt(struct via_vm* vm);
const struct via_value* via_reg_parn(struct via_vm* vm);

void via_set_pc(struct via_vm* vm, struct via_value* value);
void via_set_expr(struct via_vm* vm, const struct via_value* value);
void via_set_proc(struct via_vm* vm, const struct via_value* value);
void via_set_args(struct via_vm* vm, const struct via_value* value);
void via_set_env(struct via_vm* vm, const struct via_value* value);
void via_set_excn(struct via_vm* vm, const struct via_value* value);
void via_set_exh(struct via_vm* vm, const struct via_value* value);
void via_set_sptr(struct via_vm* vm, struct via_value* value);
void via_set_ctxt(struct via_vm* vm, const struct via_value* value);
void via_set_parn(struct via_vm* vm, const struct via_value* value);

void via_lookup_form(struct via_vm* vm);

void via_set_form(
    struct via_vm* vm,
    const struct via_value* symbol,
    const struct via_value* definition
);

void via_env_set(
    struct via_vm* vm,
    const struct via_value* symbol,
    const struct via_value* value
);

void via_b_env_set(struct via_vm* vm);

void via_garbage_collect(struct via_vm* vm);

void via_catch(
    struct via_vm* vm,
    const struct via_value* expr,
    const struct via_value* handler
);

void via_throw(struct via_vm* vm, const struct via_value* exception);

const struct via_value* via_backtrace(
    struct via_vm* vm,
    const struct via_value* frame
);

void via_default_exception_handler(struct via_vm* vm);

const struct via_value* via_run_eval(struct via_vm* vm);

const struct via_value* via_run(struct via_vm* vm);

#ifdef __cplusplus
}
#endif

