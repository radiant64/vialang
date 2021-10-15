#include <via/builtin.h>

#include "exception-strings.h"

#include <via/alloc.h>
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

static struct via_value* via_expand_lookup(
    struct via_value* symbol,
    struct via_value* meta_var
) {
    if (meta_var->v_car == symbol) {
        return meta_var->v_cdr;
    }

    return symbol;
}

static struct via_value* via_expand_recurse(
    struct via_vm* vm,
    struct via_value* meta_var,
    struct via_value* value
) {
    if (!value) {
        return NULL;
    } else if (value->type == VIA_V_SYMBOL) {
        return via_expand_lookup(value, meta_var);
    } else if (value->type == VIA_V_PAIR) {
        return via_make_pair(
            vm,
            via_expand_recurse(vm, meta_var, value->v_car),
            via_expand_recurse(vm, meta_var, value->v_cdr)
        );
    }

    return value;
}

static void via_expand_template(struct via_vm* vm) {
    // TODO: Improve address caching.
    static struct via_vm* cached_instance = NULL;
    static via_int eval_transform_proc = -1;
    if (vm != cached_instance) {
        cached_instance = vm;
        eval_transform_proc = via_asm_label_lookup(vm, "eval-transform-proc");
    }

    const struct via_value* ctxt = via_reg_ctxt(vm)->v_cdr;
    struct via_value* form = via_reg_ctxt(vm)->v_car;

    const struct via_value* formals = form->v_car;
    struct via_value* body = form->v_cdr->v_car;

    while (formals) {
        if (!ctxt) {
            via_throw(
                vm,
                via_except_syntax_error(vm, FORM_INADEQUATE_ARGS)
            );
        }
        
        body = via_expand_recurse(
            vm,
            via_make_pair(vm, formals->v_car, ctxt->v_car),
            body
        );
        formals = formals->v_cdr;
        ctxt = ctxt->v_cdr;
    }
    
    via_set_expr(vm, body);
    via_reg_pc(vm)->v_int = eval_transform_proc;
}

void via_f_syntax_transform(struct via_vm* vm) {
    // TODO: Improve address caching.
    static struct via_vm* cached_instance = NULL;
    static via_int expand_template_proc = -1;
    if (vm != cached_instance) {
        cached_instance = vm;
        expand_template_proc = via_asm_label_lookup(vm, "expand-template-proc");
    }

    const struct via_value* ctxt = via_reg_ctxt(vm)->v_cdr;

    if (!ctxt) {
        via_throw(vm, via_except_syntax_error(vm, ""));
        return;
    }
    struct via_value* symbol = ctxt->v_car;
    struct via_value* formals = ctxt->v_cdr->v_car;
    struct via_value* body = ctxt->v_cdr->v_cdr->v_car;

    via_env_set(
        vm,
        symbol,
        via_make_form(
            vm,
            formals,
            body,
            via_make_builtin(vm, expand_template_proc)
        )
    );
}

void via_f_quote(struct via_vm* vm) {
    const struct via_value* ctxt = via_reg_ctxt(vm)->v_cdr;
    if (!ctxt) {
        via_throw(vm, via_except_syntax_error(vm, ""));
        return;
    }
    vm->ret = ctxt->v_car;
}

void via_f_yield(struct via_vm* vm) {
    const struct via_value* ctxt = via_reg_ctxt(vm)->v_cdr;
    if (ctxt) {
        via_throw(vm, via_except_syntax_error(vm, ""));
        return;
    }
    struct via_value* retval = via_make_pair(vm, vm->ret, vm->regs);

    vm->acc = via_reg_parn(vm);
    via_assume_frame(vm);
    
    vm->ret = via_make_pair(vm, vm->ret, via_make_frame(vm));
}

void via_f_lambda(struct via_vm* vm) {
    const struct via_value* ctxt = via_reg_ctxt(vm)->v_cdr;
    if (!ctxt) {
        via_throw(vm, via_except_syntax_error(vm, ""));
        return;
    }
    if (ctxt->type != VIA_V_PAIR) {
        goto throw_syntax_error;
    }
    struct via_value* formals = NULL;
    struct via_value* cursor = ctxt->v_car;
    while (cursor) {
        formals = via_make_pair(vm, cursor->v_car, formals);
        cursor = cursor->v_cdr;
    }
    if (ctxt->v_cdr->type != VIA_V_PAIR) {
        goto throw_syntax_error;
    }
    struct via_value* body = ctxt->v_cdr->v_car;
    vm->ret = via_make_proc(vm, body, formals, via_reg_env(vm)); 
    if (ctxt->v_cdr->v_cdr) {
        goto throw_syntax_error;
    }

    return;

throw_syntax_error:
    via_throw(vm, via_except_syntax_error(vm, MALFORMED_LAMBDA));
}

void via_f_catch(struct via_vm* vm) {
    struct via_value* ctxt = via_reg_ctxt(vm)->v_cdr;
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

void via_p_nilp(struct via_vm* vm) {
    const struct via_value* args = via_reg_args(vm);
    if (!args || args->v_cdr) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }
    vm->ret = via_make_bool(vm, via_pop_arg(vm) == NULL);
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
    const struct via_value* args = via_reg_args(vm);
    if (!args || !args->v_cdr || args->v_cdr) {
        via_throw(vm, via_except_argument_error(vm, TWO_ARGS));
        return;
    }
    struct via_value* cdr = via_pop_arg(vm);
    struct via_value* car = via_pop_arg(vm);
    vm->ret = via_make_pair(vm, car, cdr);
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
    vm->ret = NULL;
    while (via_reg_args(vm) != NULL) {
        vm->ret = via_make_pair(vm, via_pop_arg(vm), vm->ret);
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

void via_p_str_concat(struct via_vm* vm) {
    const struct via_value* args = via_reg_args(vm);
    if (args == NULL || !args->v_cdr || args->v_cdr->v_cdr) {
        via_throw(vm, via_except_argument_error(vm, TWO_ARGS));
        return;
    }

    const struct via_value* b = via_pop_arg(vm);
    const struct via_value* a = via_pop_arg(vm);
    size_t a_len = strlen(a->v_string);
    size_t b_len = strlen(b->v_string);
    char* strval = via_malloc(a_len + b_len + 1);
    if (!strval) {
        via_throw(vm, via_except_out_of_memory(vm, ALLOC_FAIL));
        return;
    }

    memcpy(strval, a->v_string, a_len);
    memcpy(&strval[a_len], b->v_string, b_len + 1); // Include zero char.

    vm->ret = via_make_value(vm);
    vm->ret->type = VIA_V_STRING;
    vm->ret->v_string = strval;
}

void via_p_backtrace(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }
    vm->ret = via_backtrace(vm, via_pop_arg(vm));
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

    via_bind(vm, "expand-template-proc", via_expand_template); 
    via_register_form(
        vm,
        "syntax-transform",
        "syntax-transform-proc",
        via_list(
            vm,
            via_sym(vm, "&syntax-symbol"),
            via_sym(vm, "&syntax-formals"),
            via_sym(vm, "&syntax-template"),
            NULL
        ),
        via_f_syntax_transform
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
    via_register_proc(vm, "nil?", "nilp-proc", NULL, via_p_nilp);
    via_register_proc(vm, "context", "context-proc", NULL, via_p_context);
    via_register_proc(vm, "exception", "exception-proc", NULL, via_p_exception);
    via_register_proc(vm, "cons", "cons-proc", NULL, via_p_cons);
    via_register_proc(vm, "car", "car-proc", NULL, via_p_car);
    via_register_proc(vm, "cdr", "cdr-proc", NULL, via_p_cdr);
    via_register_proc(vm, "list", "list-proc", NULL, via_p_list);
    via_register_proc(vm, "display", "display-proc", NULL, via_p_display);
    via_register_proc(
        vm,
        "str-concat",
        "str-concat-proc",
        NULL,
        via_p_str_concat
    );
    via_register_proc(vm, "backtrace", "backtrace-proc", NULL, via_p_backtrace);
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

