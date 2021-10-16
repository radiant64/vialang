#include <via/vm.h>

#include "exception-strings.h"

#include <via/alloc.h>
#include <via/assembler.h>
#include <via/builtin.h>
#include <via/exceptions.h>
#include <via/parse.h>

#include <builtin-native.h>
#include <native-via.h>

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if 0
#define DPRINTF(...) do { printf(__VA_ARGS__); } while (0)
#else
#define DPRINTF(...) do {  } while (0)
#endif

#define DEFAULT_HEAPSIZE 2048 
#define DEFAULT_STACKSIZE 256
#define DEFAULT_BOUND_SIZE 2048
#define DEFAULT_PROGRAM_SIZE 512
#define DEFAULT_LABELS_CAP 16

static struct via_value* via_value_transformer(struct via_vm* vm, void* value) {
    return value;
}

static struct via_value* via_sym_transformer(struct via_vm* vm, void* value) {
    return via_sym(vm, (const char*) value);
}

static struct via_value* via_list_impl(
    struct via_vm* vm,
    va_list ap,
    struct via_value*(*transform_func)(struct via_vm*, void*)
) {
    if (!transform_func) {
        transform_func = via_value_transformer;
    }

    void* value = va_arg(ap, void*);
    if (!value) {
        return NULL;
    }

    struct via_value* list = via_make_pair(vm, transform_func(vm, value), NULL);
    struct via_value* cursor = list;
    while (value = va_arg(ap, void*)) {
        cursor->v_cdr = via_make_pair(vm, transform_func(vm, value), NULL);
        cursor = cursor->v_cdr;
    }

    return list;
}

struct via_value* via_list(struct via_vm* vm, ...) {
    va_list ap;
    va_start(ap, vm);

    struct via_value* list = via_list_impl(vm, ap, NULL);

    va_end(ap);

    return list;
}

static void via_throw_proc(struct via_vm* vm) {
    via_throw(vm, via_pop(vm));
}

static void via_env_set_proc(struct via_vm* vm) {
    struct via_value* value = via_pop_arg(vm);
    via_env_set(vm, via_pop(vm), value);
    vm->ret = value;
}

struct via_assembly_result via_add_core_routines(struct via_vm* vm) {
    vm->program[0] = VIA_OP_RETURN;
    vm->write_cursor = 1;

    // Add some builtins.
    via_bind(vm, "lookup-proc", via_env_lookup);
    via_bind(vm, "apply-proc", via_apply);
    via_bind(vm, "form-expand-proc", via_expand_form);
    via_bind(vm, "assume-proc", via_assume_frame);
    via_bind(vm, "env-set-proc", via_env_set_proc);
    via_bind(vm, "throw-proc", via_throw_proc);

    // Assemble the native routines.
    return via_assemble(vm, builtin_prg); 
}

struct via_vm* via_create_vm() {
    struct via_vm* vm = via_calloc(1, sizeof(struct via_vm));
    if (vm) {
        vm->heap = via_calloc(sizeof(struct via_value*), DEFAULT_HEAPSIZE);
        if (!vm->heap) {
            goto cleanup_vm;
        }
        vm->heap_cap = DEFAULT_HEAPSIZE;

        vm->program = via_calloc(DEFAULT_PROGRAM_SIZE, sizeof(via_opcode));
        if (!vm->program) {
            goto cleanup_heap;
        }
        vm->program_cap = DEFAULT_PROGRAM_SIZE;

        vm->labels = via_malloc(DEFAULT_LABELS_CAP * sizeof(char*));
        if (!vm->labels) {
            goto cleanup_program;
        }
        vm->label_addrs = via_malloc(DEFAULT_LABELS_CAP * sizeof(via_int));
        if (!vm->label_addrs) {
            goto cleanup_labels;
        }
        vm->labels_cap = DEFAULT_LABELS_CAP;

        vm->bound =
            via_calloc(DEFAULT_BOUND_SIZE, sizeof(void(*)(struct via_vm*)));
        if (!vm->bound) {
            goto cleanup_label_addrs;
        }
        vm->bound_cap = DEFAULT_BOUND_SIZE;

        vm->stack = via_calloc(DEFAULT_STACKSIZE, sizeof(struct via_value*));
        if (!vm->stack) {
            goto cleanup_bound;
        }
        vm->stack_size = DEFAULT_STACKSIZE;

        vm->regs = NULL;
        vm->regs = via_make_frame(vm);
        via_set_pc(vm, via_make_int(vm, 0));
        via_set_env(vm, via_make_env(vm));
        via_set_sptr(vm, via_make_int(vm, 0));

        { 
            struct via_assembly_result result = via_add_core_routines(vm);
            if (result.status != VIA_ASM_SUCCESS) {
                fprintf(
                    stderr,
                    "Assembler error!\n\tType: %s\n\tCursor: %s\n",
                    via_asm_error_string(result.status),
                    result.err_ptr
                );
            }
        }
        via_add_core_forms(vm);
        via_add_core_procedures(vm);

        struct via_value* native = via_parse(vm, native_via);
        if (!via_parse_ctx_matched(native)) {
            const char* cursor = via_parse_ctx_cursor(native);
            if (!*cursor) {
                fprintf(
                    stderr,
                    "Parse error at end of bundled code (missing paren?)\n"
                );
            } else {
                fprintf(
                    stderr,
                    "Parse error in bundled code: %s (offset %ld)\n",
                    cursor,
                    ((intptr_t) cursor) - ((intptr_t) native_via)
                );
            }
            goto cleanup_stack;
        }
        
        via_set_expr(vm, via_parse_ctx_program(native)->v_car);
        {
            struct via_value* result = via_run_eval(vm); 
            if (via_is_exception(vm, result)) {
                fprintf(
                    stderr,
                    "Exception in bundled code: %s\n",
                    via_to_string(vm, result->v_cdr)->v_string
                );
                goto cleanup_stack;
            }
        }
    }

    return vm;

cleanup_stack:
    via_free(vm->stack);

cleanup_bound:
    via_free(vm->bound);

cleanup_label_addrs:
    via_free(vm->label_addrs);

cleanup_labels:
    via_free(vm->labels);

cleanup_program:
    via_free(vm->program);

cleanup_heap:
    via_free(vm->heap);

cleanup_vm:
    via_free(vm);

    return NULL;
}

static void via_delete_value(struct via_value* value) {
    switch (value->type) {
    case VIA_V_STRING:
    case VIA_V_SYMBOL:
        via_free((char*) value->v_string);
        break;
    case VIA_V_ARRAY:
    case VIA_V_FRAME:
        via_free(value->v_arr);
        break;
    }

    via_free(value);
}

void via_free_vm(struct via_vm* vm) {
    via_free(vm->stack);
    via_free(vm->bound);
    via_free(vm->label_addrs);

    for (via_int i = 0; i < vm->labels_count; ++i) {
        via_free(vm->labels[i]);
    }
    via_free(vm->labels);

    via_free(vm->program);

    for (via_int i = 0; i < vm->heap_top; ++i) {
        if (vm->heap[i]) {
            via_delete_value(vm->heap[i]);
        }
    }
    via_free(vm->heap);

    via_free(vm);
}

struct via_value* via_make_value(struct via_vm* vm) {
    size_t i = vm->heap_free;
    for (; i < vm->heap_top + 1 && (vm->heap[i] != NULL); ++i) {
    }

    if (i == vm->heap_cap) {
        struct via_value** new_heap = via_realloc(
            vm->heap,
            sizeof(struct via_value*) * vm->heap_cap * 2
        );
        if (!new_heap) {
            return NULL;
        }
        vm->heap = new_heap;
        vm->heap_cap *= 2;
    }

    if (i > vm->heap_top) {
        vm->heap_top = i;
    }
    struct via_value* val = via_calloc(sizeof(struct via_value), 1);
    vm->heap[i] = val;
    vm->heap_free = i + 1;
    val->type = VIA_V_NIL;
    return val;
}

struct via_value* via_make_int(struct via_vm* vm, via_int v_int) {
    struct via_value* value = via_make_value(vm);
    value->type = VIA_V_INT;
    value->v_int = v_int;

    return value;
}

struct via_value* via_make_float(struct via_vm* vm, via_float v_float) {
    struct via_value* value = via_make_value(vm);
    value->type = VIA_V_FLOAT;
    value->v_float = v_float;

    return value;
}

struct via_value* via_make_bool(struct via_vm* vm, via_bool v_bool) {
    struct via_value* value = via_make_value(vm);
    value->type = VIA_V_BOOL;
    value->v_bool = v_bool;

    return value;
}

struct via_value* via_make_string(struct via_vm* vm, const char* v_string) {
    size_t size = strlen(v_string) + 1;
    char* string_copy = via_malloc(size);
    memcpy(string_copy, v_string, size);

    struct via_value* value = via_make_value(vm);
    value->type = VIA_V_STRING;
    value->v_string = string_copy;

    return value;
}

struct via_value* via_make_stringview(struct via_vm* vm, const char* v_string) {
    struct via_value* value = via_make_value(vm);
    value->type = VIA_V_STRINGVIEW;
    value->v_string = v_string;
}

struct via_value* via_make_builtin(struct via_vm* vm, via_int v_builtin) {
    struct via_value* value = via_make_value(vm);
    value->type = VIA_V_BUILTIN;
    value->v_int = v_builtin;

    return value;
}

struct via_value* via_make_pair(
    struct via_vm* vm,
    struct via_value* car,
    struct via_value* cdr
) {
    struct via_value* value = via_make_value(vm);
    value->type = VIA_V_PAIR;
    value->v_car = car;
    value->v_cdr = cdr;

    return value;
}

struct via_value* via_make_proc(
    struct via_vm* vm,
    struct via_value* body,
    struct via_value* formals,
    struct via_value* env
) {
    struct via_value* env_elem = via_make_pair(vm, env, NULL);
    struct via_value* formals_elem = via_make_pair(vm, formals, env_elem);
    struct via_value* value = via_make_pair(vm, body, formals_elem);
    value->type = VIA_V_PROC;

    return value;
}

struct via_value* via_make_form(
    struct via_vm* vm,
    struct via_value* formals,
    struct via_value* body,
    struct via_value* routine
) {
    struct via_value* value = via_make_pair(
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

struct via_value** via_make_array(struct via_vm* vm, size_t size) {
    return via_calloc(size, sizeof(struct via_value*));
}

struct via_value* via_make_frame(struct via_vm* vm) {
    struct via_value* frame = via_make_value(vm);
    if (!frame) {
        return NULL;
    }

    frame->type = VIA_V_FRAME;
    frame->v_arr = via_make_array(vm, VIA_REG_COUNT);
    if (!frame->v_arr) {
        return NULL;
    }
    frame->v_size = VIA_REG_COUNT;

    frame->v_arr[VIA_REG_PC] = via_make_value(vm);
    frame->v_arr[VIA_REG_PC]->type = VIA_V_INT;
    if (vm->regs) {
        frame->v_arr[VIA_REG_PC]->v_int = via_reg_pc(vm)->v_int;
        for (size_t i = 1; i < VIA_REG_COUNT - 1; ++i) {
            struct via_value** fval = &frame->v_arr[i];
            *fval = vm->regs->v_arr[i];
        }
    }
    frame->v_arr[VIA_REG_PARN] = vm->regs;

    if (vm->regs) {
    DPRINTF("\t\t\tExpr: %s\n", via_to_string(vm, via_reg_expr(vm))->v_string);
    }

    return frame;
}

struct via_value* via_make_env(struct via_vm* vm) {
    return via_make_pair(vm, via_reg_env(vm), NULL);
}

struct via_value* via_sym(struct via_vm* vm, const char* name) {
    struct via_value* entry;
    struct via_value* cursor = vm->symbols;
    if (!vm->symbols) {
        entry = via_make_string(vm, name);
        entry->type = VIA_V_SYMBOL;

        vm->symbols = via_make_pair(vm, entry, NULL);
        return vm->symbols->v_car;
    } else {
        bool found;
        while(
            !(found = (strcmp(cursor->v_car->v_symbol, name) == 0))
                && cursor->v_cdr
        ) {
            cursor = cursor->v_cdr;
        }
        if (found) {
            return cursor->v_car;
        }
    }

    entry = via_make_string(vm, name);
    entry->type = VIA_V_SYMBOL;

    cursor->v_cdr = via_make_pair(vm, entry, NULL);

    return cursor->v_cdr->v_car;
}

void via_return_outer(struct via_vm* vm, struct via_value* value) {
    vm->ret = value;
    vm->regs = via_reg_parn(vm);
    via_reg_pc(vm)->v_int = 0;
}

void via_assume_frame(struct via_vm* vm) {
    assert(vm->acc->type == VIA_V_FRAME);
    vm->regs = vm->acc;
}

via_int via_bind(struct via_vm* vm, const char* name, via_bindable func) {
    if (vm->bound_count == vm->bound_cap) {
        via_bindable* new_bound = via_realloc(
            vm->bound,
            sizeof(via_bindable) * vm->bound_cap * 2
        );
        if (!new_bound) {
            return -1;
        }
        vm->bound = new_bound;
        vm->bound_cap *= 2;
    }
    const via_int index = vm->bound_count++;
    vm->bound[index] = func;

    char buffer[256];
    snprintf(buffer, 256, "%s:\ncallb %" VIA_FMTId "\nreturn", name, index);
    struct via_assembly_result result = via_assemble(vm, buffer); 

    return result.addr;
};

void via_register_proc(
    struct via_vm* vm,
    const char* symbol,
    const char* asm_label,
    struct via_value* formals,
    void(*func)(struct via_vm*)
) {
    via_env_set(
        vm,
        via_sym(vm, symbol),
        via_make_proc(
            vm,
            via_make_builtin(vm, via_bind(vm, asm_label, func)),
            formals,
            via_reg_env(vm)
        )
    );
}

void via_register_native_proc(
    struct via_vm* vm,
    const char* symbol,
    const char* asm_label,
    struct via_value* formals
) {
    via_env_set(
        vm,
        via_sym(vm, symbol),
        via_make_proc(
            vm,
            via_make_builtin(vm, via_asm_label_lookup(vm, asm_label)),
            formals,
            via_reg_env(vm)
        )
    );
}

void via_register_form(
    struct via_vm* vm,
    const char* symbol,
    const char* asm_label,
    struct via_value* formals,
    void(*func)(struct via_vm*)
) {
    struct via_value* form = via_make_form(
        vm,
        formals,
        NULL,
        via_make_builtin(vm, via_bind(vm, asm_label, func))
    );

    via_env_set(vm, via_sym(vm, symbol), form);
}

void via_register_native_form(
    struct via_vm* vm,
    const char* symbol,
    const char* asm_label,
    struct via_value* formals
) {
    struct via_value* form = via_make_form(
        vm,
        formals,
        NULL,
        via_make_builtin(vm, via_asm_label_lookup(vm, asm_label))
    );

    via_env_set(vm, via_sym(vm, symbol), form);
}

struct via_value* via_get(struct via_vm* vm, const char* symbol_name) {
    struct via_value* tmp_acc = vm->acc;
    struct via_value* tmp_ret = vm->ret;

    vm->acc = via_sym(vm, symbol_name);
    via_env_lookup(vm);
    struct via_value* value = vm->ret;

    vm->acc = tmp_acc;
    vm->ret = tmp_ret;

    return value;
}

struct via_value* via_formals(struct via_vm* vm, ...) {
    va_list ap;
    va_start(ap, vm);

    const char* name;
    struct via_value* formals = NULL;
    while (name = va_arg(ap, void*)) {
        formals = via_make_pair(vm, via_sym(vm, name), formals);
    }

    va_end(ap);

    return formals;
}

static via_bool via_list_contains(
    struct via_value* list,
    struct via_value* value
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

static struct via_value* via_list_append(
    struct via_vm* vm,
    struct via_value* list,
    struct via_value* value
) {
    struct via_value* head = list;
    if (!head) {
        head = via_make_pair(vm, value, NULL);
    } else {
        for (; list->v_cdr; list = list->v_cdr) {
        }
        
        list->v_cdr = via_make_pair(vm, value, NULL);
    }

    return head;
}

#define OUT_PRINTF(...)\
    do {\
        len = snprintf(buf, 0, __VA_ARGS__) + 1;\
        buf = via_malloc(len);\
        snprintf(buf, len, __VA_ARGS__);\
        out = via_make_string(vm, buf);\
        via_free(buf);\
    } while(0);

static struct via_value* via_to_string_impl(
    struct via_vm* vm,
    struct via_value* value,
    struct via_value* sequence,
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
    struct via_value* out;
    struct via_value* car;
    struct via_value* cdr;

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
        OUT_PRINTF("\"%s\"", value->v_string);
        return out;
    case VIA_V_ARRAY:
        return via_make_stringview(vm, "<array>");
    case VIA_V_FRAME:
        return via_make_stringview(vm, "<frame>");
    case VIA_V_BUILTIN:
        OUT_PRINTF("<builtin %" VIA_FMTIx  ">", value->v_int);
        return out;
    case VIA_V_PAIR:
    default:
        if (via_list_contains(sequence, value)) {
            return via_make_stringview(vm, "<BECOMES CYCLIC>");
        }
        sequence = via_list_append(vm, sequence, value);

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

        car = via_to_string_impl(vm, value->v_car, sequence, false);
        cdr = via_to_string_impl(vm, value->v_cdr, sequence, true);
        OUT_PRINTF(fmt, car->v_string, cdr->v_string);
        return out;
    }
}

struct via_value* via_to_string(struct via_vm* vm, struct via_value* value) {
    return via_to_string_impl(vm, value, NULL, false);
}

void via_push(struct via_vm* vm, struct via_value* value) {
    const via_int top = via_reg_sptr(vm)->v_int;
    if (top == vm->stack_size) {
        struct via_value** new_stack = via_realloc(
            vm->stack,
            sizeof(struct via_value*) * vm->stack_size * 2
        );
        if (!new_stack) {
            // TODO: Throw exception.
            return;
        }
        vm->stack = new_stack;
        vm->stack_size *= 2;
    }
    vm->stack[via_reg_sptr(vm)->v_int++] = value;
}

struct via_value* via_pop(struct via_vm* vm) {
    struct via_value* val = vm->stack[--(via_reg_sptr(vm)->v_int)];
    vm->stack[via_reg_sptr(vm)->v_int] = NULL;
    return val;
}

void via_push_arg(struct via_vm* vm, struct via_value* val) {
    via_set_args(vm, via_make_pair(vm, val, via_reg_args(vm)));
}

struct via_value* via_pop_arg(struct via_vm* vm) {
    struct via_value* val = via_reg_args(vm)->v_car;
    via_set_args(vm, via_reg_args(vm)->v_cdr);
    return val; 
}

void via_apply(struct via_vm* vm) {
    // TODO: Improve address caching.
    static struct via_vm* cached_instance = NULL;
    static via_int eval_proc = -1;
    if (vm != cached_instance) {
        cached_instance = vm;
        eval_proc = via_asm_label_lookup(vm, "eval-proc");
    }

    const struct via_value* proc = via_reg_proc(vm);
    const struct via_value* args = via_reg_args(vm);

    struct via_value* formals = proc->v_cdr->v_car;
    vm->acc = proc->v_cdr->v_cdr->v_car;
    via_set_env(vm, via_make_env(vm));

    while (formals) {
        via_env_set(vm, formals->v_car, args->v_car);
        args = args->v_cdr;
        formals = formals->v_cdr;
    }

    via_set_expr(vm, proc->v_car);
    via_reg_pc(vm)->v_int = eval_proc;
}

void via_expand_form(struct via_vm* vm) {
    // TODO: Improve address caching.
    static struct via_vm* cached_instance = NULL;
    static via_int eval_proc = -1;
    if (vm != cached_instance) {
        cached_instance = vm;
        eval_proc = via_asm_label_lookup(vm, "eval-proc");
    }

    struct via_value* routine = vm->acc->v_cdr->v_cdr;

    via_set_ctxt(vm, via_make_pair(vm, vm->acc, via_reg_ctxt(vm)));
    via_set_expr(vm, routine);
    via_reg_pc(vm)->v_int = eval_proc;
}

struct via_value* via_reg_pc(struct via_vm* vm) {
    return vm->regs->v_arr[VIA_REG_PC];
}

struct via_value* via_reg_expr(struct via_vm* vm) {
    return vm->regs->v_arr[VIA_REG_EXPR];
}

struct via_value* via_reg_proc(struct via_vm* vm) {
    return vm->regs->v_arr[VIA_REG_PROC];
}

struct via_value* via_reg_args(struct via_vm* vm) {
    return vm->regs->v_arr[VIA_REG_ARGS];
}

struct via_value* via_reg_env(struct via_vm* vm) {
    return vm->regs->v_arr[VIA_REG_ENV];
}

struct via_value* via_reg_excn(struct via_vm* vm) {
    return vm->regs->v_arr[VIA_REG_EXCN];
}

struct via_value* via_reg_exh(struct via_vm* vm) {
    return vm->regs->v_arr[VIA_REG_EXH];
}

struct via_value* via_reg_sptr(struct via_vm* vm) {
    return vm->regs->v_arr[VIA_REG_SPTR];
}

struct via_value* via_reg_ctxt(struct via_vm* vm) {
    return vm->regs->v_arr[VIA_REG_CTXT];
}

struct via_value* via_reg_parn(struct via_vm* vm) {
    return vm->regs->v_arr[VIA_REG_PARN];
}

void via_set_pc(struct via_vm* vm, struct via_value* value) {
    vm->regs->v_arr[VIA_REG_PC] = value;
}

void via_set_expr(struct via_vm* vm, struct via_value* value) {
    vm->regs->v_arr[VIA_REG_EXPR] = value;
}

void via_set_proc(struct via_vm* vm, struct via_value* value) {
    vm->regs->v_arr[VIA_REG_PROC] = value;
}

void via_set_args(struct via_vm* vm, struct via_value* value) {
    vm->regs->v_arr[VIA_REG_ARGS] = value;
}

void via_set_env(struct via_vm* vm, struct via_value* value) {
    vm->regs->v_arr[VIA_REG_ENV] = value;
}

void via_set_excn(struct via_vm* vm, struct via_value* value) {
    vm->regs->v_arr[VIA_REG_EXCN] = value;
}

void via_set_exh(struct via_vm* vm, struct via_value* value) {
    vm->regs->v_arr[VIA_REG_EXH] = value;
}

void via_set_sptr(struct via_vm* vm, struct via_value* value) {
    vm->regs->v_arr[VIA_REG_SPTR] = value;
}

void via_set_ctxt(struct via_vm* vm, struct via_value* value) {
    vm->regs->v_arr[VIA_REG_CTXT] = value;
}

void via_set_parn(struct via_vm* vm, struct via_value* value) {
    vm->regs->v_arr[VIA_REG_PARN] = value;
}


void via_env_lookup(struct via_vm* vm) {
    struct via_value* env = via_reg_env(vm);
    do {
        struct via_value* cursor = env;
        if (cursor->v_cdr) {
            do {
                cursor = cursor->v_cdr;
                if (cursor->v_car->v_car == vm->acc) {
                    vm->ret = cursor->v_car->v_cdr;
                    return;
                }
            } while (cursor->v_cdr);
        }
        env = env->v_car;
    } while (env);

    vm->ret = via_make_value(vm);
    vm->ret->type = VIA_V_UNDEFINED;
}

void via_env_set(
    struct via_vm* vm,
    struct via_value* symbol,
    struct via_value* value
) {
    struct via_value* cursor = via_reg_env(vm);
    via_bool found = false;
    if (cursor->v_cdr) {
        do {
            cursor = cursor->v_cdr;
            if (cursor->v_car->v_car == symbol) {
                found = true;
            }
        } while (!found && cursor->v_cdr);
    } 
    if (!found) {
        cursor->v_cdr = via_make_pair(vm, NULL, NULL);
        cursor = cursor->v_cdr;
    }

    DPRINTF(
        "\t\t%s = %s\n",
        via_to_string(vm, symbol)->v_string,
        via_to_string(vm, value)->v_string
    );

    cursor->v_car = via_make_pair(vm, symbol, value);
}

static void via_mark(struct via_value* value, uint8_t generation) {
    if (!value || value->generation == generation) {
        return;
    }
    value->generation = generation;

    switch (value->type) {
    case VIA_V_PAIR:
    case VIA_V_PROC:
    case VIA_V_FORM:
        via_mark(value->v_car, generation);
        via_mark(value->v_cdr, generation);
        break;
    case VIA_V_FRAME:
    case VIA_V_ARRAY:
        for (size_t i = 0; i < value->v_size; ++i) {
            via_mark(value->v_arr[i], generation);
        }
        break;
    }
}

static void via_sweep(struct via_vm* vm) {
    for (size_t i = 0; i < vm->heap_cap; ++i) {
        if (vm->heap[i] && vm->heap[i]->generation != vm->generation) {
            via_delete_value(vm->heap[i]);
            vm->heap[i] = NULL;
            if (i < vm->heap_free) {
                vm->heap_free = i;
            }
        }
    }
}

void via_garbage_collect(struct via_vm* vm) {
    // Marking.

    vm->generation++;
    via_mark(vm->acc, vm->generation);
    via_mark(vm->ret, vm->generation);
    for (size_t i = 0; i < VIA_REG_COUNT; ++i) {
        via_mark(vm->regs, vm->generation);
    }
    for (size_t i = 0; vm->stack[i] && i < vm->stack_size; ++i) {
        via_mark(vm->stack[i], vm->generation);
    }
    via_mark(vm->symbols, vm->generation);

    // Sweeping.

    via_sweep(vm);
}

void via_catch(
    struct via_vm* vm,
    struct via_value* expr,
    struct via_value* handler
) {
    struct via_value* handler_frame = via_make_frame(vm);
    handler_frame->v_arr[VIA_REG_PC]->v_int = via_asm_label_lookup(
        vm,
        "eval-proc"
    ); 
    handler_frame->v_arr[VIA_REG_EXPR] = handler;

    vm->regs = via_make_frame(vm);
    via_set_exh(vm, handler_frame);
    via_set_expr(vm, expr);
}

void via_throw(struct via_vm* vm, struct via_value* exception) {
    vm->acc = via_reg_exh(vm);
    via_assume_frame(vm);
    via_set_excn(vm, exception);
}

struct via_value* via_backtrace(struct via_vm* vm, struct via_value* frame) {
    struct via_value* trace = NULL;
    while (frame) {
        trace = via_make_pair(vm, frame->v_arr[VIA_REG_EXPR], trace);
        frame = frame->v_arr[VIA_REG_PARN];
    }
    return trace;
}

void via_default_exception_handler(struct via_vm* vm) {
    struct via_value* excn = via_reg_excn(vm);
    struct via_value* frame = via_backtrace(vm, via_excn_frame(excn));
    struct via_value* exc_pair = via_make_pair(
        vm,
        via_excn_symbol(excn),
        via_excn_message(excn)
    );
    fprintf(
        stderr,
        "(via) Exception: %s\nBacktrace:\n", 
        via_to_string(vm, exc_pair)->v_string
    );
    
    while (frame) {
        fprintf(stderr, "\t%s\n", via_to_string(vm, frame->v_car)->v_string);
        frame = frame->v_cdr;
    }

    via_return_outer(vm, excn);
}

struct via_value* via_run_eval(struct via_vm* vm) {
    via_reg_pc(vm)->v_int = via_asm_label_lookup(vm, "eval-proc");
    via_catch(
        vm,
        via_reg_expr(vm),
        via_make_builtin(
            vm,
            via_bind(vm, "default-except-proc", via_default_exception_handler)
        )
    );
    // Replace the current frame with the catch clause. 
    via_set_parn(vm, NULL);

    return via_run(vm);
}

struct via_value* via_run(struct via_vm* vm) {
    struct via_value* val;
    via_int old_pc;
    via_int op;
    via_int frame = 0;
    
process_state:
    op = vm->program[via_reg_pc(vm)->v_int];
    old_pc = via_reg_pc(vm)->v_int;
    DPRINTF("PC %04" VIA_FMTIx ": ", via_reg_pc(vm)->v_int);
    switch (op & 0xff) {
    case VIA_OP_NOP:
        DPRINTF("NOP\n");
        break;
    case VIA_OP_CAR:
        DPRINTF("CAR\n");
        vm->acc = vm->acc->v_car;
        break;
    case VIA_OP_CDR:
        DPRINTF("CDR\n");
        vm->acc = vm->acc->v_cdr;
        break;
    case VIA_OP_CALL:
        DPRINTF("CALL %04" VIA_FMTIx "\n", op >> 8);
        via_reg_pc(vm)->v_int = op >> 8;
        break;
    case VIA_OP_CALLACC:
        DPRINTF("CALLACC (acc = %04" VIA_FMTIx ")\n", vm->acc->v_int);
        via_reg_pc(vm)->v_int = vm->acc->v_int;
        break;
    case VIA_OP_CALLB:
        DPRINTF("CALLB %04" VIA_FMTIx "\n", op >> 8);
        vm->bound[op >> 8](vm);
        break;
    case VIA_OP_SET:
        DPRINTF(
            "SET %" VIA_FMTId " > %s\n",
            op >> 8,
            via_to_string(vm, vm->acc)->v_string
        );
        vm->regs->v_arr[op >> 8] = vm->acc;
        break;
    case VIA_OP_LOAD:
        DPRINTF(
            "LOAD %" VIA_FMTId " < %s\n",
            op >> 8,
            via_to_string(vm, vm->regs->v_arr[op >> 8])->v_string
        );
        vm->acc = vm->regs->v_arr[op >> 8];
        break;
    case VIA_OP_LOADNIL:
        DPRINTF("LOADNIL\n");
        vm->acc = NULL;
        break;
    case VIA_OP_SETRET:
        DPRINTF("SETRET > %s\n", via_to_string(vm, vm->acc)->v_string);
        vm->ret = vm->acc;
        break;
    case VIA_OP_LOADRET:
        DPRINTF("LOADRET < %s\n", via_to_string(vm, vm->ret)->v_string);
        vm->acc = vm->ret;
        break;
    case VIA_OP_PAIRP:
        DPRINTF("PAIRP\n");
        val = via_make_value(vm);
        val->type = VIA_V_BOOL;
        if (vm->acc) {
            val->v_bool = (vm->acc->type == VIA_V_PAIR);
        } else {
            val->v_bool = false;
        }
        vm->acc = val;
        break;
    case VIA_OP_SYMBOLP:
        DPRINTF("SYMBOLP\n");
        val = via_make_value(vm);
        val->type = VIA_V_BOOL;
        if (vm->acc) {
            val->v_bool = (vm->acc->type == VIA_V_SYMBOL);
        } else {
            val->v_bool = false;
        }
        vm->acc = val;
        break;
    case VIA_OP_BUILTINP:
        DPRINTF("BUILTINP\n");
        val = via_make_value(vm);
        val->type = VIA_V_BOOL;
        if (vm->acc) {
            val->v_bool = (vm->acc->type == VIA_V_BUILTIN);
        } else {
            val->v_bool = false;
        }
        vm->acc = val;
        break;
    case VIA_OP_FORMP:
        DPRINTF("FORMP\n");
        val = via_make_value(vm);
        val->type = VIA_V_BOOL;
        if (vm->acc) {
            val->v_bool = (vm->acc->type == VIA_V_FORM);
        } else {
            val->v_bool = false;
        }
        vm->acc = val;
        break;
    case VIA_OP_FRAMEP:
        DPRINTF("FRAMEP\n");
        val = via_make_value(vm);
        val->type = VIA_V_BOOL;
        if (vm->acc) {
            val->v_bool = (vm->acc->type == VIA_V_FRAME);
        } else {
            val->v_bool = false;
        }
        vm->acc = val;
        break;
    case VIA_OP_SKIPZ:
        DPRINTF("SKIPZ\n");
        if (vm->acc && vm->acc->v_int != 0) {
            break;
        }
        via_reg_pc(vm)->v_int += (op >> 8) + 1;
        break;
    case VIA_OP_SNAP:
        DPRINTF("SNAP %" VIA_FMTId "\n", op >> 8);
        val = via_make_frame(vm);
        DPRINTF("\t\tFrame: %" VIA_FMTId "\n", ++frame);
        via_reg_pc(vm)->v_int += (op >> 8) + 1;
        vm->regs = val; 
        break;
    case VIA_OP_RETURN:
        DPRINTF("RETURN\n");
        if (!via_reg_parn(vm)) {
            return vm->ret;
        }
        vm->acc = via_reg_parn(vm);
        via_assume_frame(vm);
        DPRINTF("\t\tFrame: %" VIA_FMTId "\n", --frame);
        DPRINTF(
            "\t\tExpr: %s\n",
            via_to_string(vm, via_reg_expr(vm))->v_string
        );
        break;
    case VIA_OP_JMP:
        DPRINTF("JMP %" VIA_FMTId "\n", op >> 8);
        via_reg_pc(vm)->v_int += (op >> 8) + 1;
        break;
    case VIA_OP_PUSH:
        DPRINTF("PUSH > %s\n", via_to_string(vm, vm->acc)->v_string);
        via_push(vm, vm->acc);
        break;
    case VIA_OP_POP:
        val = via_pop(vm);
        DPRINTF("POP < %s\n", via_to_string(vm, val)->v_string);
        vm->acc = val;
        break;
    case VIA_OP_DROP:
        DPRINTF("DROP\n");
        (void) via_pop(vm);
        break;
    case VIA_OP_PUSHARG:
        DPRINTF("PUSHARG > %s\n", via_to_string(vm, vm->acc)->v_string);
        via_push_arg(vm, vm->acc);
        break;
    case VIA_OP_POPARG:
        val = via_pop_arg(vm);
        DPRINTF("POPARG < %s\n", via_to_string(vm, val)->v_string);
        vm->acc = val;
        break;
    }

    if (old_pc == via_reg_pc(vm)->v_int) {
        (via_reg_pc(vm)->v_int)++;
    } else {
        DPRINTF("-------------\n");
    }
    goto process_state;
}

