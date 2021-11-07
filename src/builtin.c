#include <via/builtin.h>

#include "exception-strings.h"

#include <via/alloc.h>
#include <via/assembler.h>
#include <via/exceptions.h>
#include <via/parse.h>
#include <via/port.h>
#include <via/type-utils.h>
#include <via/vm.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OP(INTOP, FLOATOP)\
    if (!via_reg_args(vm) || !via_reg_args(vm)->v_cdr) {\
        via_throw(vm, via_except_argument_error(vm, TWO_ARGS));\
        return;\
    }\
    const struct via_value* a = via_pop_arg(vm);\
    const struct via_value* b = via_pop_arg(vm);\
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
    const struct via_value* a = via_pop_arg(vm);\
    const struct via_value* b = via_pop_arg(vm);\
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

static const struct via_value* via_expand_lookup(
    const struct via_value* symbol,
    const struct via_value* meta_var
) {
    if (meta_var->v_car == symbol) {
        return meta_var->v_cdr;
    }

    return symbol;
}

static const struct via_value* via_expand_recurse(
    struct via_vm* vm,
    const struct via_value* meta_var,
    const struct via_value* value
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
    const struct via_value* form = via_reg_ctxt(vm)->v_car;

    const struct via_value* formals = form->v_car;
    const struct via_value* body = form->v_cdr->v_car;

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
    const struct via_value* symbol = ctxt->v_car;
    const struct via_value* formals = ctxt->v_cdr->v_car;
    const struct via_value* body = ctxt->v_cdr->v_cdr->v_car;

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
//    struct via_value* retval = via_make_pair(vm, vm->ret, vm->regs);

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
    const struct via_value* formals = ctxt->v_car;
    if (ctxt->v_cdr->type != VIA_V_PAIR) {
        goto throw_syntax_error;
    }
    const struct via_value* body = ctxt->v_cdr->v_car;
    vm->ret = via_make_proc(vm, body, formals, via_reg_env(vm)); 
    if (ctxt->v_cdr->v_cdr) {
        goto throw_syntax_error;
    }

    return;

throw_syntax_error:
    via_throw(vm, via_except_syntax_error(vm, MALFORMED_LAMBDA));
}

void via_f_catch(struct via_vm* vm) {
    // TODO: Improve address caching.
    static struct via_vm* cached_instance = NULL;
    static via_int eval_proc = -1;
    if (vm != cached_instance) {
        cached_instance = vm;
        eval_proc = via_asm_label_lookup(vm, "eval-proc");
    }

    const struct via_value* ctxt = via_reg_ctxt(vm)->v_cdr;
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
    via_reg_pc(vm)->v_int = eval_proc;
}

void via_p_eval(struct via_vm* vm) {
    // TODO: Improve address caching.
    static struct via_vm* cached_instance = NULL;
    static via_int eval_proc = -1;
    if (vm != cached_instance) {
        cached_instance = vm;
        eval_proc = via_asm_label_lookup(vm, "eval-proc");
    }

    via_set_env(vm, via_reg_env(vm)->v_car);
    via_set_expr(vm, via_pop_arg(vm));
    via_reg_pc(vm)->v_int = eval_proc;
}

void via_p_parse(struct via_vm* vm) {
    const struct via_value* args = via_reg_args(vm);
    if (!args || args->v_cdr) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* source = via_pop_arg(vm);
    if (source->type != VIA_V_STRING && source->type != VIA_V_STRINGVIEW) {
        via_throw(vm, via_except_invalid_type(vm, STRING_REQUIRED));
        return;
    }

    const struct via_value* result = via_parse(vm, source->v_string);
    if (!via_parse_success(result)) {
        via_throw(
            vm,
            via_except_syntax_error(vm, via_parse_ctx_cursor(result))
        );
        return;
    }

    vm->ret = via_parse_ctx_program(result)->v_car;
}

void via_p_throw(struct via_vm* vm) {
    const struct via_value* excn = via_pop_arg(vm);
    if (!excn || via_reg_args(vm)) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }
    if (!via_is_exception(vm, excn)) {
        via_throw(vm, via_except_invalid_type(vm, EXC_TYPE_ERROR));
        return;
    }

    via_throw(vm, excn);
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

void via_p_not(struct via_vm* vm) {
    const struct via_value* arg = via_pop_arg(vm);
    if (!arg || via_reg_args(vm)) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }
    if (arg->type != VIA_V_BOOL) {
        via_throw(vm, via_except_invalid_type(vm, BOOL_REQUIRED));
        return;
    }
    vm->ret = via_make_bool(vm, !arg->v_bool);
}

void via_p_context(struct via_vm* vm) {
    if (via_reg_args(vm)) {
        via_throw(vm, via_except_argument_error(vm, NO_ARGS));
        return;
    }
    vm->ret = via_reg_ctxt(vm);
}

void via_p_make_exception(struct via_vm* vm) {
    const struct via_value* args = via_reg_args(vm);
    if (!args || !args->v_cdr || args->v_cdr->v_cdr) {
        via_throw(vm, via_except_argument_error(vm, TWO_ARGS));
        return;
    }
    const struct via_value* symbol = via_pop_arg(vm);
    if (symbol->type != VIA_V_SYMBOL) {
        via_throw(vm, via_except_invalid_type(vm, SYMBOL_REQUIRED));
        return;
    }
    const struct via_value* message = via_pop_arg(vm);
    if (message->type != VIA_V_STRING && message->type != VIA_V_STRINGVIEW) {
        via_throw(vm, via_except_invalid_type(vm, STRING_REQUIRED));
        return;
    }

    vm->ret = via_make_exception(vm, symbol->v_symbol, message->v_string);
}

void via_p_exception(struct via_vm* vm) {
    if (via_reg_args(vm)) {
        via_throw(vm, via_except_argument_error(vm, NO_ARGS));
        return;
    }
    vm->ret = via_reg_excn(vm);
}

void via_p_exception_type(struct via_vm* vm) {
    const struct via_value* excn = via_pop_arg(vm);
    if (!excn || via_reg_args(vm)) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }
    if (!via_is_exception(vm, excn)) {
        via_throw(vm, via_except_invalid_type(vm, EXC_TYPE_ERROR));
        return;
    }

    vm->ret = via_excn_symbol(excn);
}

void via_p_exception_message(struct via_vm* vm) {
    const struct via_value* excn = via_pop_arg(vm);
    if (!excn || via_reg_args(vm)) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }
    if (!via_is_exception(vm, excn)) {
        via_throw(vm, via_except_invalid_type(vm, EXC_TYPE_ERROR));
        return;
    }

    vm->ret = via_excn_message(excn);
}

void via_p_exception_frame(struct via_vm* vm) {
    const struct via_value* excn = via_pop_arg(vm);
    if (!excn || via_reg_args(vm)) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }
    if (!via_is_exception(vm, excn)) {
        via_throw(vm, via_except_invalid_type(vm, EXC_TYPE_ERROR));
        return;
    }

    vm->ret = via_excn_frame(excn);
}

void via_p_cons(struct via_vm* vm) {
    const struct via_value* args = via_reg_args(vm);
    if (!args || (args->v_cdr && args->v_cdr->v_cdr)) {
        via_throw(vm, via_except_argument_error(vm, TWO_ARGS));
        return;
    }
    const struct via_value* car = via_pop_arg(vm);
    const struct via_value* cdr = via_pop_arg(vm);
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
    vm->ret = via_reg_args(vm);
}

void via_p_reverse_list(struct via_vm* vm) {
    const struct via_value* args = via_reg_args(vm);
    if (args == NULL || args->v_cdr) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }
    vm->ret = via_reverse_list(vm, via_pop_arg(vm));
}

void via_p_str_concat(struct via_vm* vm) {
    const struct via_value* args = via_reg_args(vm);
    if (args == NULL || !args->v_cdr || args->v_cdr->v_cdr) {
        via_throw(vm, via_except_argument_error(vm, TWO_ARGS));
        return;
    }

    const struct via_value* a = via_pop_arg(vm);
    const struct via_value* b = via_pop_arg(vm);
    
    vm->ret = via_string_concat(vm, a, b);
}

void via_p_debug(struct via_vm* vm) {
    printf("DEBUG: %s\n", via_to_string(vm, via_pop_arg(vm))->v_string);
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
    const struct via_value* a = via_pop_arg(vm);
    const struct via_value* b = via_pop_arg(vm);
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

void via_p_byte(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr != NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* num = via_pop_arg(vm);

    if (num->type != VIA_V_INT) {
        via_throw(vm, via_except_invalid_type(vm, INT_REQUIRED));
        return;
    }

    if (num->v_int < -128 || num->v_int > 255) {
        via_throw(vm, via_except_runtime_error(vm, OUT_OF_RANGE));
        return;
    }

    vm->ret = via_make_int(vm, num->v_int & 0xff);
}

void via_p_int(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr != NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* num = via_pop_arg(vm);

    switch (num->type) {
    case VIA_V_INT:
        vm->ret = num;
        break;
    case VIA_V_FLOAT:
        vm->ret = via_make_int(vm, (via_int) num->v_float);
        break;
    case VIA_V_BOOL:
        vm->ret = via_make_int(vm, num->v_bool ? 1 : 0);
        break;
    case VIA_V_STRING:
    case VIA_V_STRINGVIEW:
        // TODO: Handle errors.
        vm->ret = via_make_int(vm, strtol(num->v_string, NULL, 0));
        break;
    default:
        via_throw(vm, via_except_invalid_type(vm, CANT_CONVERT));
    }
}

void via_p_float(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr != NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* num = via_pop_arg(vm);

    switch (num->type) {
    case VIA_V_INT:
        vm->ret = via_make_float(vm, (via_float) num->v_int);
        break;
    case VIA_V_FLOAT:
        vm->ret = num;
        break;
    case VIA_V_BOOL:
        vm->ret = via_make_float(vm, num->v_bool ? 1.0 : 0.0);
        break;
    case VIA_V_STRING:
    case VIA_V_STRINGVIEW:
        // TODO: Handle errors.
        vm->ret = via_make_float(vm, strtod(num->v_string, NULL));
        break;
    default:
        via_throw(vm, via_except_invalid_type(vm, CANT_CONVERT));
    }
}

void via_p_string(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr != NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    vm->ret = via_to_string_raw(vm, via_pop_arg(vm));
}

void via_p_bytep(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr != NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* arg = via_pop_arg(vm);
    vm->ret = via_make_bool(
        vm,
        arg->type == VIA_V_INT && arg->v_int >= -128 && arg->v_int < 256
    );
}

void via_p_nilp(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr != NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* arg = via_pop_arg(vm);
    vm->ret = via_make_bool(vm, arg == NULL);
}

void via_p_intp(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr != NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* arg = via_pop_arg(vm);
    vm->ret = via_make_bool(vm, arg->type == VIA_V_INT);
}

void via_p_floatp(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr != NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* arg = via_pop_arg(vm);
    vm->ret = via_make_bool(vm, arg->type == VIA_V_FLOAT);
}

void via_p_boolp(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr != NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* arg = via_pop_arg(vm);
    vm->ret = via_make_bool(vm, arg->type == VIA_V_BOOL);
}

void via_p_stringp(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr != NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* arg = via_pop_arg(vm);
    vm->ret = via_make_bool(vm, arg->type == VIA_V_STRING);
}

void via_p_stringviewp(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr != NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* arg = via_pop_arg(vm);
    vm->ret = via_make_bool(vm, arg->type == VIA_V_STRINGVIEW);
}

void via_p_pairp(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr != NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* arg = via_pop_arg(vm);
    vm->ret = via_make_bool(vm, arg != NULL && arg->type == VIA_V_PAIR);
}

void via_p_arrayp(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr != NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* arg = via_pop_arg(vm);
    vm->ret = via_make_bool(vm, arg->type == VIA_V_ARRAY);
}

void via_p_procp(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr != NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* arg = via_pop_arg(vm);
    vm->ret = via_make_bool(vm, arg->type == VIA_V_PROC);
}

void via_p_formp(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr != NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* arg = via_pop_arg(vm);
    vm->ret = via_make_bool(vm, arg->type == VIA_V_FORM);
}

void via_p_builtinp(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr != NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* arg = via_pop_arg(vm);
    vm->ret = via_make_bool(vm, arg->type == VIA_V_BUILTIN);
}

void via_p_framep(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr != NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* arg = via_pop_arg(vm);
    vm->ret = via_make_bool(vm, arg->type == VIA_V_FRAME);
}

void via_p_symbolp(struct via_vm* vm) {
    if (via_reg_args(vm) == NULL || via_reg_args(vm)->v_cdr != NULL) {
        via_throw(vm, via_except_argument_error(vm, ONE_ARG));
        return;
    }

    const struct via_value* arg = via_pop_arg(vm);
    vm->ret = via_make_bool(vm, arg->type == VIA_V_SYMBOL);
}

void via_add_core_forms(struct via_vm* vm) {
    via_register_native_form(vm, "begin", "begin-proc", NULL);
    via_register_native_form(vm, "if", "if-proc", NULL);
    via_register_native_form(vm, "and", "and-proc", NULL);
    via_register_native_form(vm, "or", "or-proc", NULL);
    via_register_native_form(vm, "set!", "set-proc", NULL);

    via_bind(vm, "expand-template-proc", (via_bindable) via_expand_template); 
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
        (via_bindable) via_f_syntax_transform
    );
    via_register_form(
        vm,
        "quote",
        "quote-proc",
        NULL,
        (via_bindable) via_f_quote
    );
    via_register_form(
        vm,
        "yield",
        "yield-proc",
        NULL,
        (via_bindable) via_f_yield
    );
    via_register_form(
        vm,
        "lambda",
        "lambda-proc",
        NULL,
        (via_bindable) via_f_lambda
    );
    via_register_form(
        vm,
        "catch",
        "catch-proc",
        NULL,
        (via_bindable) via_f_catch
    );
}

void via_add_core_procedures(struct via_vm* vm) {
    via_register_proc(
        vm,
        "eval",
        "eval-native-proc",
        NULL,
        (via_bindable) via_p_eval
    );
    via_register_proc(
        vm,
        "parse",
        "parse-proc",
        NULL,
        (via_bindable) via_p_parse
    );
    via_register_proc(
        vm,
        "throw",
        "throw-proc",
        NULL,
        (via_bindable) via_p_throw
    );
    via_register_proc(vm, "=", "eq-proc", NULL, (via_bindable) via_p_eq);
    via_register_proc(vm, ">", "gt-proc", NULL, (via_bindable) via_p_gt);
    via_register_proc(vm, "<", "lt-proc", NULL, (via_bindable) via_p_lt);
    via_register_proc(vm, ">=", "gte-proc", NULL, (via_bindable) via_p_gte);
    via_register_proc(vm, "<=", "lte-proc", NULL, (via_bindable) via_p_lte);
    via_register_proc(vm, "<>", "neq-proc", NULL, (via_bindable) via_p_neq);
    via_register_proc(vm, "not", "not-proc", NULL, (via_bindable) via_p_not);
    via_register_proc(
        vm,
        "context",
        "context-proc",
        NULL,
        (via_bindable) via_p_context
    );
    via_register_proc(
        vm,
        "make-exception",
        "make-exception-proc",
        NULL,
        (via_bindable) via_p_make_exception
    );
    via_register_proc(
        vm,
        "exception",
        "exception-proc",
        NULL,
        (via_bindable) via_p_exception
    );
    via_register_proc(
        vm,
        "exception-type",
        "exception-type-proc",
        NULL,
        (via_bindable) via_p_exception_type
    );
    via_register_proc(
        vm,
        "exception-message",
        "exception-message-proc",
        NULL,
        (via_bindable) via_p_exception_message
    );
    via_register_proc(
        vm,
        "exception-frame",
        "exception-frame-proc",
        NULL,
        (via_bindable) via_p_exception_frame
    );
    via_register_proc(vm, "cons", "cons-proc", NULL, (via_bindable) via_p_cons);
    via_register_proc(vm, "car", "car-proc", NULL, (via_bindable) via_p_car);
    via_register_proc(vm, "cdr", "cdr-proc", NULL, (via_bindable) via_p_cdr);
    via_register_proc(vm, "list", "list-proc", NULL, (via_bindable) via_p_list);
    via_register_proc(
        vm,
        "reverse",
        "reverse-proc",
        NULL,
        (via_bindable) via_p_reverse_list
    );
    via_register_proc(
        vm,
        "str-concat",
        "str-concat-proc",
        NULL,
        (via_bindable) via_p_str_concat
    );
    via_register_proc(
        vm,
        "debug",
        "debug-proc",
        NULL,
        (via_bindable) via_p_debug
    );
    via_register_proc(
        vm,
        "backtrace",
        "backtrace-proc",
        NULL,
        (via_bindable) via_p_backtrace
    );
    via_register_proc(vm, "+", "add-proc", NULL, (via_bindable) via_p_add);
    via_register_proc(vm, "-", "sub-proc", NULL, (via_bindable) via_p_sub);
    via_register_proc(vm, "*", "mul-proc", NULL, (via_bindable) via_p_mul);
    via_register_proc(vm, "/", "div-proc", NULL, (via_bindable) via_p_div);
    via_register_proc(vm, "%", "mod-proc", NULL, (via_bindable) via_p_mod);
    via_register_proc(vm, "^", "pow-proc", NULL, (via_bindable) via_p_pow);
    via_register_proc(vm, "sin", "sin-proc", NULL, (via_bindable) via_p_sin);
    via_register_proc(vm, "cos", "cos-proc", NULL, (via_bindable) via_p_cos);
    via_register_proc(
        vm,
        "garbage-collect",
        "gc-proc",
        NULL,
        (via_bindable) via_p_garbage_collect
    );
    via_register_proc(vm, "byte", "byte-proc", NULL, (via_bindable) via_p_byte);
    via_register_proc(vm, "int", "int-proc", NULL, (via_bindable) via_p_int);
    via_register_proc(
        vm,
        "float",
        "float-proc",
        NULL,
        (via_bindable) via_p_float
    );
    via_register_proc(
        vm,
        "string",
        "string-proc",
        NULL,
        (via_bindable) via_p_string
    );
    via_register_proc(
        vm,
        "byte?",
        "bytep-proc",
        NULL,
        (via_bindable) via_p_bytep
    );
    via_register_proc(vm, "nil?", "nilp-proc", NULL, (via_bindable) via_p_nilp);
    via_register_proc(vm, "int?", "intp-proc", NULL, (via_bindable) via_p_intp);
    via_register_proc(
        vm,
        "bool?",
        "boolp-proc",
        NULL,
        (via_bindable) via_p_boolp
    );
    via_register_proc(
        vm,
        "string?",
        "stringp-proc",
        NULL,
        (via_bindable) via_p_stringp
    );
    via_register_proc(
        vm,
        "stringview?",
        "stringviewp-proc",
        NULL,
        (via_bindable) via_p_stringviewp
    );
    via_register_proc(
        vm,
        "pair?",
        "pairp-proc",
        NULL,
        (via_bindable) via_p_pairp
    );
    via_register_proc(
        vm,
        "array?",
        "arrayp-proc",
        NULL,
        (via_bindable) via_p_arrayp
    );
    via_register_proc(
        vm,
        "proc?",
        "procp-proc",
        NULL,
        (via_bindable) via_p_procp
    );
    via_register_proc(
        vm,
        "form?",
        "formp-proc",
        NULL,
        (via_bindable) via_p_formp
    );
    via_register_proc(
        vm,
        "builtin?",
        "builtinp-proc",
        NULL,
        (via_bindable) via_p_builtinp
    );
    via_register_proc(
        vm,
        "frame?",
        "framep-proc",
        NULL,
        (via_bindable) via_p_framep
    );
    via_register_proc(
        vm,
        "symbol?",
        "symbolp-proc",
        NULL,
        (via_bindable) via_p_symbolp
    );

    via_add_port_procedures(vm);
}

