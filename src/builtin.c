#include <via/builtin.h>

#include <via/asm_macros.h>
#include <via/vm.h>

#include <assert.h>
#include <math.h>

void via_b_quote(struct via_vm* vm) {
    vm->ret = via_context(vm)->v_car;
}

void via_b_begin(struct via_vm* vm) { 
    vm->regs[VIA_REG_EXPR] = via_context(vm);
    vm->regs[VIA_REG_PC]->v_int = VIA_BEGIN_PROC;
}

void via_b_yield(struct via_vm* vm) {
    if (!vm->frames_top) {
        // TODO: Throw exception
    }
    vm->acc = vm->frames[--vm->frames_top];
    via_assume_frame(vm);
    
    vm->ret = via_make_pair(vm, vm->ret, via_make_frame(vm));
}

void via_b_if(struct via_vm* vm) {
    vm->regs[VIA_REG_PC]->v_int = VIA_IF_PROC;
}

void via_b_context(struct via_vm* vm) {
    vm->ret = via_context(vm);
}

void via_apply(struct via_vm* vm) {
    const struct via_value* proc = vm->regs[VIA_REG_PROC];
    const struct via_value* args = vm->regs[VIA_REG_ARGS];

    struct via_value* formals = proc->v_cdr->v_car;
    vm->acc = proc->v_cdr->v_cdr->v_car;
    vm->regs[VIA_REG_ENV] = via_make_env(vm);

    while (formals) {
        via_env_set(vm, formals->v_car, args->v_car);
        args = args->v_cdr;
        formals = formals->v_cdr;
    }

    vm->regs[VIA_REG_EXPR] = proc->v_car;
    vm->regs[VIA_REG_PC]->v_int = VIA_EVAL_PROC;
}

void via_b_cons(struct via_vm* vm) {
    vm->ret = via_make_pair(vm, via_get(vm, "a"), via_get(vm, "b"));
}

void via_b_car(struct via_vm* vm) {
    const struct via_value* p = via_get(vm, "p");
    vm->ret = p->v_car;
}

void via_b_cdr(struct via_vm* vm) {
    const struct via_value* p = via_get(vm, "p");
    vm->ret = p->v_cdr;
}

void via_b_list(struct via_vm* vm) {
    vm->ret = vm->regs[VIA_REG_ARGS];
}

#define OP(INTOP, FLOATOP)\
    const struct via_value* a = via_get(vm, "a");\
    const struct via_value* b = via_get(vm, "b");\
    assert(a->type == VIA_V_INT || a->type == VIA_V_FLOAT);\
    assert(b->type == VIA_V_INT || b->type == VIA_V_FLOAT);\
    if (a->type == VIA_V_FLOAT || b->type == VIA_V_FLOAT) {\
        vm->ret = via_make_float(vm, FLOATOP);\
    } else {\
        vm->ret = via_make_int(vm, INTOP);\
    }

#define INFIX_OP(OPERATOR)\
    OP(\
        a->v_int OPERATOR b->v_int,\
        (a->type == VIA_V_INT ? a->v_int : a->v_float)\
            OPERATOR (b->type == VIA_V_INT ? b->v_int : b->v_float)\
    )

#define PREFIX_OP(OPERATOR)\
    OP(\
        OPERATOR(a->v_int, b->v_int),\
        OPERATOR(\
            a->type == VIA_V_INT ? a->v_int : a->v_float,\
            b->type == VIA_V_INT ? b->v_int : b->v_float\
        )\
    )

void via_b_add(struct via_vm* vm) {
    INFIX_OP(+)
}

void via_b_sub(struct via_vm* vm) {
    INFIX_OP(-)
}

void via_b_mul(struct via_vm* vm) {
    INFIX_OP(*)
}

void via_b_div(struct via_vm* vm) {
    INFIX_OP(/)
}

void via_b_mod(struct via_vm* vm) {
    const struct via_value* a = via_get(vm, "a");
    const struct via_value* b = via_get(vm, "b");
    assert(a->type == VIA_V_INT && b->type == VIA_V_INT);
    vm->ret = via_make_int(vm, a->v_int % b->v_int);
}

void via_b_pow(struct via_vm* vm) {
    PREFIX_OP(pow)
}

void via_b_sin(struct via_vm* vm) {
    const struct via_value* a = via_get(vm, "a");
    assert(a->type == VIA_V_FLOAT);
    vm->ret = via_make_float(vm, sin(a->v_float));
}

void via_b_cos(struct via_vm* vm) {
    const struct via_value* a = via_get(vm, "a");
    assert(a->type == VIA_V_FLOAT);
    vm->ret = via_make_float(vm, cos(a->v_float));
}

void via_add_core_forms(struct via_vm* vm) {
    via_register_form(vm, "quote", via_b_quote);
    via_register_form(vm, "begin", via_b_begin);
    via_register_form(vm, "yield", via_b_yield);
    via_register_form(vm, "if", via_b_if);
}

void via_add_core_procedures(struct via_vm* vm) {
    via_register_proc(vm, "context", NULL, via_b_context);
    via_register_proc(vm, "garbage-collect", NULL, via_garbage_collect);
    via_register_proc(vm, "cons", via_formals(vm, "a", "b", NULL), via_b_cons);
    via_register_proc(vm, "car", via_formals(vm, "p", NULL), via_b_car);
    via_register_proc(vm, "cdr", via_formals(vm, "p", NULL), via_b_cdr);
    via_register_proc(vm, "list", NULL, via_b_list);
    via_register_proc(vm, "+", via_formals(vm, "a", "b", NULL), via_b_add);
    via_register_proc(vm, "-", via_formals(vm, "a", "b", NULL), via_b_sub);
    via_register_proc(vm, "*", via_formals(vm, "a", "b", NULL), via_b_mul);
    via_register_proc(vm, "/", via_formals(vm, "a", "b", NULL), via_b_div);
    via_register_proc(vm, "%", via_formals(vm, "a", "b", NULL), via_b_mod);
    via_register_proc(vm, "^", via_formals(vm, "a", "b", NULL), via_b_pow);
    via_register_proc(vm, "sin", via_formals(vm, "a", NULL), via_b_sin);
    via_register_proc(vm, "cos", via_formals(vm, "a", NULL), via_b_cos);
}

