#include <via/type-utils.h>

#include "exception-strings.h"

#include <via/alloc.h>
#include <via/exceptions.h>
#include <via/port.h>
#include <via/value.h>
#include <via/vm.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const struct via_value* via_value_transformer(
    struct via_vm* vm,
    void* value
) {
    return value;
}

static const struct via_value* via_sym_transformer(
    struct via_vm* vm,
    void* value
) {
    return via_sym(vm, (const char*) value);
}

static const struct via_value* via_list_impl(
    struct via_vm* vm,
    va_list ap,
    const struct via_value*(*transform_func)(struct via_vm*, void*)
) {
    if (!transform_func) {
        transform_func = via_value_transformer;
    }

    void* value = va_arg(ap, void*);
    if (!value) {
        return NULL;
    }

    const struct via_value* list =
        via_make_pair(vm, transform_func(vm, value), NULL);
    struct via_value* cursor = (struct via_value*) list;
    while (value = va_arg(ap, void*)) {
        cursor->v_cdr = via_make_pair(vm, transform_func(vm, value), NULL);
        cursor = (struct via_value*) cursor->v_cdr;
    }

    return list;
}

static via_bool via_list_contains(
    const struct via_value* list,
    const struct via_value* value
) {
    if (!list) {
        return false;
    }

    do {
        if (value == list->v_car) {
            return true;
        }
    } while (list && (list = list->v_cdr));

    return false;
}

const struct via_value* via_make_int(struct via_vm* vm, via_int v_int) {
    struct via_value* value = via_make_value(vm);
    value->type = VIA_V_INT;
    value->v_int = v_int;

    return value;
}

const struct via_value* via_make_float(struct via_vm* vm, via_float v_float) {
    struct via_value* value = via_make_value(vm);
    value->type = VIA_V_FLOAT;
    value->v_float = v_float;

    return value;
}

const struct via_value* via_make_bool(struct via_vm* vm, via_bool v_bool) {
    struct via_value* value = via_make_value(vm);
    value->type = VIA_V_BOOL;
    value->v_bool = v_bool;

    return value;
}

const struct via_value* via_make_string(
    struct via_vm* vm,
    const char* v_string
) {
    size_t size = strlen(v_string) + 1;
    char* string_copy = via_malloc(size);
    memcpy(string_copy, v_string, size);

    struct via_value* value = via_make_value(vm);
    value->type = VIA_V_STRING;
    value->v_string = string_copy;

    return value;
}

const struct via_value* via_make_stringview(
    struct via_vm* vm,
    const char* v_string
) {
    struct via_value* value = via_make_value(vm);
    value->type = VIA_V_STRINGVIEW;
    value->v_string = v_string;

    return value;
}

const struct via_value* via_make_builtin(struct via_vm* vm, via_int v_builtin) {
    struct via_value* value = via_make_value(vm);
    value->type = VIA_V_BUILTIN;
    value->v_int = v_builtin;

    return value;
}

const struct via_value* via_make_pair(
    struct via_vm* vm,
    const struct via_value* car,
    const struct via_value* cdr
) {
    struct via_value* value = via_make_value(vm);
    value->type = VIA_V_PAIR;
    value->v_car = car;
    value->v_cdr = cdr;

    return value;
}

const struct via_value* via_make_proc(
    struct via_vm* vm,
    const struct via_value* body,
    const struct via_value* formals,
    const struct via_value* env
) {
    const struct via_value* env_elem = via_make_pair(vm, env, NULL);
    const struct via_value* formals_elem = via_make_pair(vm, formals, env_elem);
    struct via_value* value = (struct via_value*) via_make_pair(
        vm,
        body,
        formals_elem
    );
    value->type = VIA_V_PROC;

    return value;
}

const struct via_value* via_make_form(
    struct via_vm* vm,
    const struct via_value* formals,
    const struct via_value* body,
    const struct via_value* routine
) {
    struct via_value* value = (struct via_value*) via_make_pair(
        vm,
        formals,
        via_make_pair(
            vm,
            body,
            routine
        )
    );
    value->type = VIA_V_FORM;

    return value;
}

const struct via_value* via_make_handle(struct via_vm* vm, void* handle) {
    struct via_value* value = via_make_value(vm);
    value->type = VIA_V_HANDLE;
    value->v_handle = handle;

    return value;
}

const struct via_value** via_make_array(struct via_vm* vm, size_t size) {
    return via_calloc(size, sizeof(struct via_value*));
}

const struct via_value* via_make_env(
    struct via_vm* vm,
    const struct via_value* parent
) {
    return via_make_pair(vm, parent, NULL);
}

const struct via_value* via_escape_string(
    struct via_vm* vm,
    const struct via_value* value
) {
    // Allocate space for potentially escaping every character in the string.
    char* target = via_calloc(1, strlen(value->v_string) * 2);
    if (!target) {
        return NULL;
    }

    const char* c = value->v_string;
    char* dest = target;
    while (*c) {
        switch (*c) {
        case '\"':
            *(dest++) = '\\';
            *(dest++) = '\"';
            break;
        case '\n':
            *(dest++) = '\\';
            *(dest++) = 'n';
            break;
        case '\r':
            *(dest++) = '\\';
            *(dest++) = 'r';
            break;
        case '\t':
            *(dest++) = '\\';
            *(dest++) = 't';
            break;
        default:
            *(dest++) = *c;
        }
        c++;
    }

    const struct via_value* escaped = via_make_string(vm, target);
    via_free(target);

    return escaped;
}

#define OUT_PRINTF(...)\
    do {\
        len = snprintf(buf, 0, __VA_ARGS__) + 1;\
        buf = via_malloc(len);\
        snprintf(buf, len, __VA_ARGS__);\
        out = via_make_string(vm, buf);\
        via_free(buf);\
    } while(0);

static const struct via_value* via_to_string_impl(
    struct via_vm* vm,
    const struct via_value* value,
    const struct via_value* sequence,
    via_bool encode,
    via_bool tail
) {
    if (!value) {
        return tail
            ? via_make_stringview(vm, "")
            : via_make_stringview(vm, "()");
    }

    char* buf;
    size_t len;
    size_t offs;
    const struct via_value* out;
    const struct via_value* car;
    const struct via_value* cdr;

    const char* fmt = "%s %s";
    const char* fmt_open = "(%s %s";
    const char* fmt_dot = "%s . %s)";
    const char* fmt_pair = "(%s . %s)";
    const char* fmt_close = "%s)%s";
    const char* fmt_open_close = "(%s)%s";

    switch (value->type) {
    case VIA_V_SYMBOL:
        return value;
    case VIA_V_INVALID:
        return via_make_stringview(vm, "<INVALID>");
    case VIA_V_UNDEFINED:
        return via_make_stringview(vm, "<UNDEFINED>");
    case VIA_V_NIL:
        return via_make_stringview(vm, "<nil>");
    case VIA_V_INT:
        OUT_PRINTF("%" VIA_FMTId, value->v_int);
        return out;
    case VIA_V_FLOAT:
        OUT_PRINTF("%f", value->v_float);
        return out;
    case VIA_V_BOOL:
        return via_make_stringview(vm, value->v_bool ? "#t" : "#f");
    case VIA_V_STRING:
    case VIA_V_STRINGVIEW:
        if (encode) {
            OUT_PRINTF("\"%s\"", via_escape_string(vm, value)->v_string);
        } else {
            OUT_PRINTF("%s", value->v_string);
        }
        return out;
    case VIA_V_ARRAY:
        return via_make_stringview(vm, "<array>");
    case VIA_V_FRAME:
        return via_make_stringview(vm, "<frame>");
    case VIA_V_BUILTIN:
        OUT_PRINTF("<builtin %" VIA_FMTIx  ">", value->v_int);
        return out;
    case VIA_V_HANDLE:
        OUT_PRINTF("<handle %p>", value->v_handle);
        return out;
    case VIA_V_PROC:
    case VIA_V_FORM:
        // Don't print the entire environment.
        sequence = via_make_pair(vm, value->v_cdr->v_cdr, sequence);
        // Fall through.
    case VIA_V_PAIR:
    default:
        if (via_list_contains(sequence, value)) {
            return via_make_stringview(vm, "<...>)");
        }
        sequence = via_make_pair(vm, value, sequence);

        if (!tail) {
            if (value->v_cdr && value->v_cdr->type == VIA_V_PAIR) {
                fmt = fmt_open;
            } else if (value->v_cdr) {
                fmt = fmt_pair;
            } else {
                fmt = fmt_open_close;
            }
        } else if (value->v_cdr && value->v_cdr->type != VIA_V_PAIR) {
            fmt = fmt_dot;
        } else if (!value->v_cdr) {
            fmt = fmt_close;
        }

        car = via_to_string_impl(vm, value->v_car, sequence, encode, false);
        cdr = via_to_string_impl(vm, value->v_cdr, sequence, encode, true);
        OUT_PRINTF(fmt, car->v_string, cdr->v_string);
        return out;
    }
}

const struct via_value* via_to_string(
    struct via_vm* vm,
    const struct via_value* value
) {
    return via_to_string_impl(vm, value, NULL, true, false);
}

const struct via_value* via_to_string_raw(
    struct via_vm* vm,
    const struct via_value* value
) {
    return via_to_string_impl(vm, value, NULL, false, false);
}

const struct via_value* via_string_concat(
    struct via_vm* vm,
    const struct via_value* a,
    const struct via_value* b
) {
    size_t a_len = strlen(a->v_string);
    size_t b_len = strlen(b->v_string);

    char* strval = via_malloc(a_len + b_len + 1);
    if (!strval) {
        via_throw(vm, via_except_out_of_memory(vm, ALLOC_FAIL));
        return NULL;
    }

    memcpy(strval, a->v_string, a_len);
    memcpy(&strval[a_len], b->v_string, b_len + 1); // Include zero char.

    struct via_value* val = via_make_value(vm);
    val->type = VIA_V_STRING;
    val->v_string = strval;
    
    return val;
}

const struct via_value* via_list(struct via_vm* vm, ...) {
    va_list ap;
    va_start(ap, vm);

    const struct via_value* list = via_list_impl(vm, ap, NULL);

    va_end(ap);

    return list;
}

const struct via_value* via_formals(struct via_vm* vm, ...) {
    va_list ap;
    va_start(ap, vm);

    const struct via_value* formals =
        via_list_impl(vm, ap, via_sym_transformer);

    va_end(ap);

    return formals;
}

const struct via_value* via_reverse_list(
    struct via_vm* vm,
    const struct via_value* list
) {
    const struct via_value* new_list = NULL;

    while (list) {
        new_list = via_make_pair(vm, list->v_car, new_list);
        list = list->v_cdr;
    }

    return new_list;
}

