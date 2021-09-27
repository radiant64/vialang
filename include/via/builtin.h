#pragma once

#include <via/defs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct via_vm;

void via_f_quote(struct via_vm* vm);

void via_f_begin(struct via_vm* vm);

void via_f_yield(struct via_vm* vm);

void via_f_if(struct via_vm* vm);

void via_f_lambda(struct via_vm* vm);

void via_f_set(struct via_vm* vm);

void via_p_eq(struct via_vm* vm);

void via_p_context(struct via_vm* vm);

void via_p_exception(struct via_vm* vm);

void via_p_cons(struct via_vm* vm);

void via_p_car(struct via_vm* vm);

void via_p_cdr(struct via_vm* vm);

void via_p_list(struct via_vm* vm);

void via_p_display(struct via_vm* vm);

void via_p_add(struct via_vm* vm);

void via_p_sub(struct via_vm* vm);

void via_p_mul(struct via_vm* vm);

void via_p_div(struct via_vm* vm);

void via_p_mod(struct via_vm* vm);

void via_p_pow(struct via_vm* vm);

void via_p_sin(struct via_vm* vm);

void via_p_cos(struct via_vm* vm);

void via_p_garbage_collect(struct via_vm* vm);

void via_add_core_forms(struct via_vm* vm);

void via_add_core_procedures(struct via_vm* vm);

#ifdef __cplusplus
}
#endif

