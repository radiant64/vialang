#include <via/builtin.h>

#include <via/assembler.h>
#include <via/vm.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>

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

#define COMPARISON(OPERATOR)\
    const struct via_value* a = via_get(vm, "a");\
    const struct via_value* b = via_get(vm, "b");\
    assert(a->type == VIA_V_INT || a->type == VIA_V_FLOAT);\
    assert(b->type == VIA_V_INT || b->type == VIA_V_FLOAT);\
    if (\
        a->type >= VIA_V_INT && b->type >= VIA_V_INT\
            && a->type <= VIA_V_FLOAT && b->type <= VIA_V_FLOAT\
    ) {\
        vm->ret = via_make_bool(\
            vm,\
            (a->type == VIA_V_INT ? a->v_int : a->v_float)\
                OPERATOR (b->type == VIA_V_INT ? b->v_int : b->v_float)\
        );\
    } else {\
        vm->ret = via_make_bool(vm, a OPERATOR b);\
    }

void via_f_quote(struct via_vm* vm) {
    vm->ret = via_context(vm)->v_car;
}

void via_f_yield(struct via_vm* vm) {
    struct via_value* retval = via_make_pair(vm, vm->ret, vm->regs);

    vm->acc = vm->regs->v_arr[VIA_REG_PARN];
    via_assume_frame(vm);
    
    vm->ret = via_make_pair(vm, vm->ret, via_make_frame(vm));
}

void via_f_lambda(struct via_vm* vm) {
    struct via_value* formals = NULL;
    struct via_value* cursor = via_context(vm)->v_car;
    while (cursor) {
        formals = via_make_pair(vm, cursor->v_car, formals);
        cursor = cursor->v_cdr;
    }
    struct via_value* body = via_context(vm)->v_cdr->v_car;
    vm->ret = via_make_proc(vm, body, formals, vm->regs->v_arr[VIA_REG_ENV]); 
}

void via_f_catch(struct via_vm* vm) {
    via_catch(vm, via_context(vm)->v_car, via_context(vm)->v_cdr->v_car);
}

void via_p_eq(struct via_vm* vm) {
    COMPARISON(==)
}

void via_p_context(struct via_vm* vm) {
    vm->ret = via_context(vm);
}

void via_p_exception(struct via_vm* vm) {
    vm->ret = via_exception(vm);
}

void via_p_cons(struct via_vm* vm) {
    vm->ret = via_make_pair(vm, via_get(vm, "a"), via_get(vm, "b"));
}

void via_p_car(struct via_vm* vm) {
    const struct via_value* p = via_get(vm, "p");
    vm->ret = p->v_car;
}

void via_p_cdr(struct via_vm* vm) {
    const struct via_value* p = via_get(vm, "p");
    vm->ret = p->v_cdr;
}

void via_p_list(struct via_vm* vm) {
    vm->ret = vm->regs->v_arr[VIA_REG_ARGS];
}

void via_p_display(struct via_vm* vm) {
    fprintf(stdout, "%s\n", via_to_string(vm, via_pop_arg(vm))->v_string);
}

void via_p_add(struct via_vm* vm) {
    INFIX_OP(+)
}

void via_p_sub(struct via_vm* vm) {
    INFIX_OP(-)
}

void via_p_mul(struct via_vm* vm) {
    INFIX_OP(*)
}

void via_p_div(struct via_vm* vm) {
    INFIX_OP(/)
}

void via_p_mod(struct via_vm* vm) {
    const struct via_value* a = via_get(vm, "a");
    const struct via_value* b = via_get(vm, "b");
    assert(a->type == VIA_V_INT && b->type == VIA_V_INT);
    vm->ret = via_make_int(vm, a->v_int % b->v_int);
}

void via_p_pow(struct via_vm* vm) {
    PREFIX_OP(pow)
}

void via_p_sin(struct via_vm* vm) {
    const struct via_value* a = via_get(vm, "a");
    assert(a->type == VIA_V_FLOAT);
    vm->ret = via_make_float(vm, sin(a->v_float));
}

void via_p_cos(struct via_vm* vm) {
    const struct via_value* a = via_get(vm, "a");
    assert(a->type == VIA_V_FLOAT);
    vm->ret = via_make_float(vm, cos(a->v_float));
}

void via_p_garbage_collect(struct via_vm* vm) {
    via_garbage_collect(vm);
}

void via_add_core_forms(struct via_vm* vm) {
    via_register_native_form(vm, "begin", "begin-proc");
    via_register_native_form(vm, "if", "if-proc");
    via_register_native_form(vm, "set!", "set-proc");

    via_register_form(vm, "quote", "quote-proc", via_f_quote);
    via_register_form(vm, "yield", "yield-proc", via_f_yield);
    via_register_form(vm, "lambda", "lambda-proc", via_f_lambda);
}

void via_add_core_procedures(struct via_vm* vm) {
    struct via_value* binary_formals = via_formals(vm, "a", "b", NULL);
    struct via_value* unary_formals = via_formals(vm, "a", NULL);
    via_register_proc(vm, "=", "eq-proc", binary_formals, via_p_eq);
    via_register_proc(vm, "context", "context-proc", NULL, via_p_context);
    via_register_proc(vm, "exception", "exception-proc", NULL, via_p_exception);
    via_register_proc(vm, "cons", "cons-proc", binary_formals, via_p_cons);
    via_register_proc(vm, "car", "car-proc", binary_formals, via_p_car);
    via_register_proc(vm, "cdr", "cdr-proc", binary_formals, via_p_cdr);
    via_register_proc(vm, "list", "list-proc", NULL, via_p_list);
    via_register_proc(vm, "display", "display-proc", NULL, via_p_display);
    via_register_proc(vm, "+", "add-proc", binary_formals, via_p_add);
    via_register_proc(vm, "-", "sub-proc", binary_formals, via_p_sub);
    via_register_proc(vm, "*", "mul-proc", binary_formals, via_p_mul);
    via_register_proc(vm, "/", "div-proc", binary_formals, via_p_div);
    via_register_proc(vm, "%", "mod-proc", binary_formals, via_p_mod);
    via_register_proc(vm, "^", "pow-proc", binary_formals, via_p_pow);
    via_register_proc(vm, "sin", "sin-proc", unary_formals, via_p_sin);
    via_register_proc(vm, "cos", "cos-proc", unary_formals, via_p_cos);
    via_register_proc(
        vm,
        "garbage-collect",
        "gc-proc",
        NULL,
        via_p_garbage_collect
    );
}

