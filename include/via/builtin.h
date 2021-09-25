#pragma once

#include <via/defs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct via_vm;

void via_b_quote(struct via_vm* vm);

void via_b_begin(struct via_vm* vm);

void via_b_yield(struct via_vm* vm);

void via_b_if(struct via_vm* vm);

void via_b_context(struct via_vm* vm);

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

void via_add_core_forms(struct via_vm* vm);

void via_add_core_procedures(struct via_vm* vm);

#ifdef __cplusplus
}
#endif

