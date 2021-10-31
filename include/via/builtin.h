#pragma once

#include <via/defs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct via_vm;

void via_f_syntax_transform(struct via_vm* vm);

void via_f_quote(struct via_vm* vm);

void via_f_yield(struct via_vm* vm);

void via_f_lambda(struct via_vm* vm);

void via_f_set(struct via_vm* vm);

void via_f_catch(struct via_vm* vm);

void via_p_eval(struct via_vm* vm);

void via_p_throw(struct via_vm* vm);

void via_p_eq(struct via_vm* vm);

void via_p_gt(struct via_vm* vm);

void via_p_lt(struct via_vm* vm);

void via_p_gte(struct via_vm* vm);

void via_p_lte(struct via_vm* vm);

void via_p_neq(struct via_vm* vm);

void via_p_context(struct via_vm* vm);

void via_p_exception(struct via_vm* vm);

void via_p_exception_type(struct via_vm* vm);

void via_p_exception_message(struct via_vm* vm);

void via_p_exception_frame(struct via_vm* vm);

void via_p_cons(struct via_vm* vm);

void via_p_car(struct via_vm* vm);

void via_p_cdr(struct via_vm* vm);

void via_p_list(struct via_vm* vm);

void via_p_print(struct via_vm* vm);

void via_p_display(struct via_vm* vm);

void via_p_read(struct via_vm* vm);

void via_p_str_concat(struct via_vm* vm);

void via_p_add(struct via_vm* vm);

void via_p_sub(struct via_vm* vm);

void via_p_mul(struct via_vm* vm);

void via_p_div(struct via_vm* vm);

void via_p_mod(struct via_vm* vm);

void via_p_pow(struct via_vm* vm);

void via_p_sin(struct via_vm* vm);

void via_p_cos(struct via_vm* vm);

void via_p_garbage_collect(struct via_vm* vm);

void via_p_byte(struct via_vm* vm);

void via_p_int(struct via_vm* vm);

void via_p_float(struct via_vm* vm);

void via_p_string(struct via_vm* vm);

void via_p_bytep(struct via_vm* vm);

void via_p_nilp(struct via_vm* vm);

void via_p_intp(struct via_vm* vm);

void via_p_floatp(struct via_vm* vm);

void via_p_boolp(struct via_vm* vm);

void via_p_stringp(struct via_vm* vm);

void via_p_stringviewp(struct via_vm* vm);

void via_p_pairp(struct via_vm* vm);

void via_p_arrayp(struct via_vm* vm);

void via_p_procp(struct via_vm* vm);

void via_p_formp(struct via_vm* vm);

void via_p_builtinp(struct via_vm* vm);

void via_p_framep(struct via_vm* vm);

void via_p_symbolp(struct via_vm* vm);

void via_add_core_forms(struct via_vm* vm);

void via_add_core_procedures(struct via_vm* vm);

#ifdef __cplusplus
}
#endif

