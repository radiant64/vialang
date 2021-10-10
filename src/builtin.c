#include <via/builtin.h>

#include "exception-strings.h"

#include <via/assembler.h>
#include <via/exceptions.h>
#include <via/vm.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define OP(INTOP, FLOATOP)\
    if (!via_reg_args(vm) || !via_reg_args(vm)->v_cdr) {\
        via_throw(vm, via_except_argument_error(vm, TWO_ARGS));\
        return;\
    }\
    const struct via_value* b = via_pop_arg(vm);\
    const struct via_value* a = via_pop_arg(vm);\
    if (\
        (a->type < VIA_V_INT || a->type > VIA_V_FLOAT)\
            || (b->type < VIA_V_INT || b->type > VIA_V_FLOAT)\
    ) {\
        via_throw(vm, via_except_invalid_type(vm, ARITH_NUMERIC));\
        return;\
    }\
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
    if (!via_reg_args(vm) || !via_reg_args(vm)->v_cdr) {\
        via_throw(vm, via_except_argument_error(vm, TWO_ARGS));\
        return;\
    }\
    const struct via_value* b = via_pop_arg(vm);\
    const struct via_value* a = via_pop_arg(vm);\
    if (\
        a->type >= VIA_V_INT && b->type >= VIA_V_INT\
            && a->type <= VIA_V_FLOAT && b->type <= VIA_V_FLOAT\
    ) {\
        vm->ret = via_make_bool(\
            vm,\
            (a->type == VIA_V_INT ? a->v_int : a->v_float)\
                OPERATOR (b->type == VIA_V_INT ? b->v_int : b->v_float)\
        );\
    } else if (a->type == b->type && a->type == VIA_V_BOOL) {\
        vm->ret = via_make_bool(vm, a->v_bool OPERATOR b->v_bool);\
    } else if (\
        a->type == b->type\
            && (a->type == VIA_V_STRING || a->type == VIA_V_STRINGVIEW)\
    ) {\
        vm->ret = via_make_bool(\
            vm,\
            strcmp(a->v_string, b->v_string) OPERATOR 0\
        );\
    } else {\
        vm->ret = via_make_bool(vm, a OPERATOR b);\
    }

void via_f_syntax_template(struct via_vm* vm) {
    // TODO: Improve address caching.
    if (!via_reg_ctxt(vm) ) {
        via_throw(vm, via_except_syntax_error(vm, ""));
        return;
    }
    if (via_reg_ctxt(vm)->type != VIA_V_PAIR) {
        via_throw(vm, via_except_syntax_error(vm, MALFORMED_SYNTAX));
        return;
    }
    struct via_value* symbol = via_reg_ctxt(vm)->v_car;
    struct via_value* formals = via_reg_ctxt(vm)->v_cdr->v_car;
    struct via_value* body = via_reg_ctxt(vm)->v_cdr->v_cdr->v_car;

    via_env_set(vm, symbol, via_make_form( vm, formals, body));
}

void via_f_quote(struct via_vm* vm) {
    if (via_reg_ctxt(vm)->type != VIA_V_PAIR) {
        via_throw(vm, via_except_syntax_error(vm, ""));
        return;
    }
    vm->ret = via_reg_ctxt(vm)->v_car;
}

void via_f_yield(struct via_vm* vm) {
    if (via_reg_ctxt(vm)) {
        via_throw(vm, via_except_syntax_error(vm, ""));
        return;
    }
    struct via_value* retval = via_make_pair(vm, vm->ret, vm->regs);

    vm->acc = via_reg_parn(vm);
    via_assume_frame(vm);
    
    vm->ret = via_make_pair(vm, vm->ret, via_make_frame(vm));
}

void via_f_lambda(struct via_vm* vm) {
    if (!via_reg_ctxt(vm)) {
        via_throw(vm, via_except_syntax_error(vm, ""));
        return;
    }
    if (via_reg_ctxt(vm)->type != VIA_V_PAIR) {
        goto throw_syntax_error;
    }
    struct via_value* formals = NULL;
    struct via_value* cursor = via_reg_ctxt(vm)->v_car;
    while (cursor) {
        formals = via_make_pair(vm, cursor->v_car, formals);
        cursor = cursor->v_cdr;
    }
    if (via_reg_ctxt(vm)->v_cdr->type != VIA_V_PAIR) {
        goto throw_syntax_error;
    }
    struct via_value* body = via_reg_ctxt(vm)->v_cdr->v_car;
    vm->ret = via_make_proc(vm, body, formals, via_reg_env(vm)); 
    if (via_reg_ctxt(vm)->v_cdr->v_cdr) {
        goto throw_syntax_error;
    }

    return;

throw_syntax_error:
    via_throw(vm, via_except_syntax_error(vm, MALFORMED_LAMBDA));
}

void via_f_catch(struct via_vm* vm) {
    struct via_value* ctxt = via_reg_ctxt(vm);
    if (!ctxt) {
        via_throw(vm, via_except_syntax_error(vm, ""));
        return;
    }
    if (
        ctxt->type != VIA_V_PAIR
            || !ctxt->v_cdr
            || ctxt->v_cdr->type != VIA_V_PAIR
    ) {
        via_throw(vm, via_except_syntax_error(vm, MALFORMED_CATCH));
        return;
    }
    via_catch(vm, ctxt->v_car, ctxt->v_cdr->v_car);
}

void via_p_eq(struct via_vm* vm) {
    COMPARISON(==)
}

void via_p_gt(struct via_vm* vm) {
    COMPARISON(>)
}

void via_p_lt(struct via_vm* vm) {
    COMPARISON(<)
}

void via_p_gte(struct via_vm* vm) {
    COMPARISON(>=)
}

void via_p_lte(struct via_vm* vm) {
    COMPARISON(<=)
}

void via_p_neq(struct via_vm* vm) {
    COMPARISON(!=)
}

void via_p_context(struct via_vm* vm) {
    if (via_reg_args(vm)) {
        via_throw(vm, via_except_argument_error(vm, NO_ARGS));
        return;
    }
    vm->ret = via_reg_ctxt(vm);
}

void via_p_exception(struct via_vm* vm) {
    if (via_reg_args(vm)) {
        via_throw(vm, via_except_argument_error(vm, NO_ARGS));
        return;
    }
    vm->ret = via_reg_excn(vm);
}

void via_p_cons(struct via_vm* vm) {
    if (!via_reg_args(vm) || !via_reg_args(vm)->v_cdr) {
        via_throw(vm, via_except_argument_error(vm, TWO_ARGS));
        return;
    }
    vm->ret = via_make_pair(vm, via_pop_arg(vm), via_pop_arg(vm));
}

void via_p_car(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }
    const struct via_value* p = via_pop_arg(vm);
    if (!p || p->type != VIA_V_PAIR) {
        via_throw(vm, via_except_invalid_type(vm, PAIR_ARG));
        return;
    }
    vm->ret = p->v_car;
}

void via_p_cdr(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }
    const struct via_value* p = via_pop_arg(vm);
    if (!p || p->type != VIA_V_PAIR) {
        via_throw(vm, via_except_invalid_type(vm, PAIR_ARG));
        return;
    }
    vm->ret = p->v_cdr;
}

void via_p_list(struct via_vm* vm) {
    struct via_value* arg;
    vm->ret = NULL;
    for (; via_reg_args(vm) != NULL; arg = via_pop_arg(vm)) {
        vm->ret = via_make_pair(vm, arg, vm->ret);
    }
}

void via_p_display(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }
    struct via_value* list = NULL;
    for (; via_reg_args(vm); list = via_make_pair(vm, via_pop_arg(vm), list)) {
    }

    while (list) {
        fprintf(stdout, "%s", via_to_string(vm, list->v_car)->v_string);
        list = list->v_cdr;
    }
    fprintf(stdout, "\n");
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
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr == NULL) {
        via_throw(vm, via_except_argument_error(vm, TWO_ARGS));
        return;
    }
    const struct via_value* b = via_pop_arg(vm);
    const struct via_value* a = via_pop_arg(vm);
    if (a->type != VIA_V_INT || b->type != VIA_V_INT) {
        via_throw(vm, via_except_invalid_type(vm, MODULO_INT));
        return;
    }
    vm->ret = via_make_int(vm, a->v_int % b->v_int);
}

void via_p_pow(struct via_vm* vm) {
    PREFIX_OP(pow)
}

void via_p_sin(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr != NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }
    const struct via_value* a = via_pop_arg(vm);
    if (a->type != VIA_V_FLOAT) {
        via_throw(vm, via_except_invalid_type(vm, FLOAT_ARG));
    }
    vm->ret = via_make_float(vm, sin(a->v_float));
}

void via_p_cos(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr != NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }
    const struct via_value* a = via_pop_arg(vm);
    if (a->type != VIA_V_FLOAT) {
        via_throw(vm, via_except_invalid_type(vm, FLOAT_ARG));
    }
    vm->ret = via_make_float(vm, cos(a->v_float));
}

void via_p_garbage_collect(struct via_vm* vm) {
    if (via_reg_args(vm)) {
        via_throw(vm, via_except_argument_error(vm, NO_ARGS));
        return;
    }
    via_garbage_collect(vm);
}

void via_add_core_forms(struct via_vm* vm) {
    via_register_native_form(vm, "begin", "begin-proc", NULL);
    via_register_native_form(vm, "if", "if-proc", NULL);
    via_register_native_form(vm, "and", "and-proc", NULL);
    via_register_native_form(vm, "or", "or-proc", NULL);
    via_register_native_form(vm, "set!", "set-proc", NULL);

    via_register_form(
        vm,
        "syntax-template",
        "syntax-template-proc",
        via_list(
            vm,
            via_sym(vm, "&syntax-symbol"),
            via_sym(vm, "&syntax-formals"),
            via_sym(vm, "&syntax-template"),
            NULL
        ),
        via_f_syntax_template
    );
    via_register_form(vm, "quote", "quote-proc", NULL, via_f_quote);
    via_register_form(vm, "yield", "yield-proc", NULL, via_f_yield);
    via_register_form(vm, "lambda", "lambda-proc", NULL, via_f_lambda);
}

void via_add_core_procedures(struct via_vm* vm) {
    via_register_proc(vm, "=", "eq-proc", NULL, via_p_eq);
    via_register_proc(vm, ">", "gt-proc", NULL, via_p_gt);
    via_register_proc(vm, "<", "lt-proc", NULL, via_p_lt);
    via_register_proc(vm, ">=", "gte-proc", NULL, via_p_gte);
    via_register_proc(vm, "<=", "lte-proc", NULL, via_p_lte);
    via_register_proc(vm, "<>", "neq-proc", NULL, via_p_neq);
    via_register_proc(vm, "context", "context-proc", NULL, via_p_context);
    via_register_proc(vm, "exception", "exception-proc", NULL, via_p_exception);
    via_register_proc(vm, "cons", "cons-proc", NULL, via_p_cons);
    via_register_proc(vm, "car", "car-proc", NULL, via_p_car);
    via_register_proc(vm, "cdr", "cdr-proc", NULL, via_p_cdr);
    via_register_proc(vm, "list", "list-proc", NULL, via_p_list);
    via_register_proc(vm, "display", "display-proc", NULL, via_p_display);
    via_register_proc(vm, "+", "add-proc", NULL, via_p_add);
    via_register_proc(vm, "-", "sub-proc", NULL, via_p_sub);
    via_register_proc(vm, "*", "mul-proc", NULL, via_p_mul);
    via_register_proc(vm, "/", "div-proc", NULL, via_p_div);
    via_register_proc(vm, "%", "mod-proc", NULL, via_p_mod);
    via_register_proc(vm, "^", "pow-proc", NULL, via_p_pow);
    via_register_proc(vm, "sin", "sin-proc", NULL, via_p_sin);
    via_register_proc(vm, "cos", "cos-proc", NULL, via_p_cos);
    via_register_proc(
        vm,
        "garbage-collect",
        "gc-proc",
        NULL,
        via_p_garbage_collect
    );
}

