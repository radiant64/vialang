#include <via/vm.h>

#include "native.h"

#include <via/alloc.h>
#include <via/asm_macros.h>
#include <via/builtin.h>

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if 0
#define DPRINTF(...) do { printf(__VA_ARGS__); } while (0)
#else
#define DPRINTF(...) do {  } while (0)
#endif

#define SEGMENT_SIZE 32
#define DEFAULT_STACKSIZE 256
#define DEFAULT_BOUND_SIZE 2048
#define DEFAULT_PROGRAM_SIZE 512
#define DEFAULT_LABELS_CAP 16

static struct via_segment* via_create_segment(size_t size) {
    struct via_segment* segment = via_calloc(1, sizeof(struct via_segment));
    if (segment) {
        segment->values = via_calloc(size, sizeof(struct via_value));
        if (!segment->values) {
            via_free(segment);
            return NULL;
        }

        segment->num_values = size;
    }

    return segment;
}

static void via_free_segments(struct via_segment* segment) {
    while (segment) {
        struct via_segment* next = segment->next;
        via_free(segment->values);
        via_free(segment);
        segment = next;
    }
}

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

void via_add_core_routines(struct via_vm* vm) {
    // Make sure PC 0 == RETURN.
    vm->program[0] = _RETURN();

    // Add some builtins.
    const via_int lookup_index = vm->num_bound++;
    vm->bound[lookup_index] = via_env_lookup;
    const via_int apply_index = vm->num_bound++;
    vm->bound[apply_index] = via_apply;
    const via_int assume_index = vm->num_bound++;
    vm->bound[assume_index] = via_assume_frame;

    // Copy the native routines into memory.
    memcpy(&vm->program[VIA_EVAL_PROC], via_eval_prg, via_eval_prg_size); 
    memcpy(
        &vm->program[VIA_EVAL_COMPOUND_PROC],
        via_eval_compound_prg,
        via_eval_compound_prg_size
    );
    memcpy(&vm->program[VIA_BEGIN_PROC], via_begin_prg, via_begin_prg_size);

    // Create trampolines for the builtins.
    vm->program[VIA_LOOKUP_PROC] = _CALLB(lookup_index);
    vm->program[VIA_LOOKUP_PROC + 1] = _RETURN();
    vm->program[VIA_APPLY_PROC] = _CALLB(apply_index);
    vm->program[VIA_APPLY_PROC + 1] = _RETURN();
    vm->program[VIA_ASSUME_PROC] = _CALLB(assume_index);
}

struct via_vm* via_create_vm() {
    struct via_vm* vm = via_calloc(1, sizeof(struct via_vm));
    if (vm) {
        vm->heap = via_create_segment(SEGMENT_SIZE);
        if (!vm->heap) {
            goto cleanup_vm;
        }

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
        vm->regs->v_arr[VIA_REG_PC] = via_make_int(vm, VIA_EVAL_PROC);
        vm->regs->v_arr[VIA_REG_ENV] = via_make_env(vm);
        vm->regs->v_arr[VIA_REG_SPTR] = via_make_int(vm, 0);

        via_add_core_routines(vm);
        via_add_core_forms(vm);
        via_add_core_procedures(vm);
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
    via_free_segments(vm->heap);

cleanup_vm:
    via_free(vm);

    return NULL;
}

struct via_value* via_make_value(struct via_vm* vm) {
    struct via_segment* segment = vm->heap;

    while (segment->count == segment->num_values && segment->next) {
        segment = segment->next;
    }

    if (segment->count == segment->num_values) {
        segment->next = via_create_segment(segment->num_values);
        if (!segment->next) {
            return NULL;
        }

        segment = segment->next;
    }

    size_t i;
    for (i = 0; segment->values[i].type != VIA_V_INVALID; ++i) {
    }

    struct via_value* val = &segment->values[i];
    val->type = VIA_V_NIL;
    val->v_car = NULL;
    val->v_cdr = NULL;
    segment->count++;
    return val;
}

void via_free_vm(struct via_vm* vm) {
    via_free_segments(vm->heap);
    via_free(vm);
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

static struct via_value* via_make_builtin(
    struct via_vm* vm,
    via_int v_builtin
) {
    struct via_value* value = via_make_value(vm);
    value->type = VIA_V_BUILTIN;
    value->v_int = v_builtin;

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
        frame->v_arr[VIA_REG_PC]->v_int = vm->regs->v_arr[VIA_REG_PC]->v_int;
        for (size_t i = 1; i < VIA_REG_COUNT - 1; ++i) {
            struct via_value** fval = &frame->v_arr[i];
            *fval = vm->regs->v_arr[i];
        }
    }
    frame->v_arr[VIA_REG_PARN] = vm->regs;

    return frame;
}

struct via_value* via_make_env(struct via_vm* vm) {
    return via_make_pair(vm, vm->regs->v_arr[VIA_REG_ENV], NULL);
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
    vm->regs = vm->regs->v_arr[VIA_REG_PARN];
    vm->regs->v_arr[VIA_REG_PC]->v_int = 0;
}

void via_assume_frame(struct via_vm* vm) {
    assert(vm->acc->type == VIA_V_FRAME);
    vm->regs = vm->acc;
}

via_int via_bind(struct via_vm* vm, void(*func)(struct via_vm*)) {
    const via_int index = vm->num_bound++;
    vm->bound[index] = func;

    const via_int routine = vm->write_cursor;
    vm->program[routine] = _CALLB(index);
    vm->program[routine + 1] = _RETURN();
    vm->write_cursor = routine + 2;

    return routine;
};

void via_register_proc(
    struct via_vm* vm,
    const char* symbol,
    struct via_value* formals,
    void(*func)(struct via_vm*)
) {
    via_env_set(
        vm,
        via_sym(vm, symbol),
        via_make_proc(
            vm,
            via_make_builtin(vm, via_bind(vm, func)),
            formals,
            vm->regs->v_arr[VIA_REG_ENV]
        )
    );
}

void via_register_form(
    struct via_vm* vm,
    const char* symbol,
    void(*func)(struct via_vm*)
) {
    struct via_value* form = via_make_pair(
        vm,
        via_make_builtin(vm, via_bind(vm, func)),
        NULL
    );
    form->type = VIA_V_FORM;

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

#define OUT_PRINTF(...)\
    do {\
        len = snprintf(buf, 0, __VA_ARGS__) + 1;\
        buf = via_malloc(len);\
        snprintf(buf, len, __VA_ARGS__);\
        out = via_make_string(vm, buf);\
        via_free(buf);\
    } while(0);

struct via_value* via_to_string(struct via_vm* vm, struct via_value* value) {
    if (!value) {
        return via_make_stringview(vm, "()");
    }

    char* buf;
    size_t len;
    size_t offs;
    struct via_value* out;
    struct via_value* list;
    struct via_value* cursor;
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
        if (value->v_cdr->type == VIA_V_PAIR) {
            len = 1;
            list = via_make_pair(vm, via_to_string(vm, value->v_car), NULL);
            len += strlen(list->v_car->v_string) + 1;
            cursor = list;
            while (value->v_cdr) {
                value = value->v_cdr;
                cursor->v_cdr = via_make_pair(
                    vm,
                    via_to_string(vm, value->v_car),
                    NULL
                );
                cursor = cursor->v_cdr;
                len += strlen(cursor->v_car->v_string) + 1;
            }
            buf = via_malloc(len);
            
            buf[0] = '(';
            offs = 1;
            cursor = list;
            while (cursor) {
                offs += snprintf(
                    &buf[offs],
                    len - offs,
                    "%s ",
                    cursor->v_car->v_string
                );
                cursor = cursor->v_cdr;
            };
            buf[len - 1] = ')';
            buf[len] = '\0';

            out = via_make_string(vm, buf);
            via_free(buf);

            return out;
        }
        OUT_PRINTF(
            "(%s %s)",
            via_to_string(vm, value->v_car)->v_string,
            via_to_string(vm, value->v_cdr)->v_string
        );
        return out;
    }
}

void via_push(struct via_vm* vm, struct via_value* value) {
    vm->stack[vm->regs->v_arr[VIA_REG_SPTR]->v_int++] = value;
}

struct via_value* via_pop(struct via_vm* vm) {
    return vm->stack[--(vm->regs->v_arr[VIA_REG_SPTR]->v_int)];
}

void via_push_arg(struct via_vm* vm, struct via_value* val) {
    vm->regs->v_arr[VIA_REG_ARGS] = via_make_pair(
        vm,
        val,
        vm->regs->v_arr[VIA_REG_ARGS]
    );
}

struct via_value* via_pop_arg(struct via_vm* vm) {
    struct via_value* val = vm->regs->v_arr[VIA_REG_ARGS]->v_car;
    vm->regs->v_arr[VIA_REG_ARGS] = vm->regs->v_arr[VIA_REG_ARGS]->v_cdr;
    return val; 
}

void via_apply(struct via_vm* vm) {
    const struct via_value* proc = vm->regs->v_arr[VIA_REG_PROC];
    const struct via_value* args = vm->regs->v_arr[VIA_REG_ARGS];

    struct via_value* formals = proc->v_cdr->v_car;
    vm->acc = proc->v_cdr->v_cdr->v_car;
    vm->regs->v_arr[VIA_REG_ENV] = via_make_env(vm);

    while (formals) {
        via_env_set(vm, formals->v_car, args->v_car);
        args = args->v_cdr;
        formals = formals->v_cdr;
    }

    vm->regs->v_arr[VIA_REG_EXPR] = proc->v_car;
    vm->regs->v_arr[VIA_REG_PC]->v_int = VIA_EVAL_PROC;
}

struct via_value* via_context(struct via_vm* vm) {
    return vm->regs->v_arr[VIA_REG_CTXT];
}

struct via_value* via_exception(struct via_vm* vm) {
    return vm->regs->v_arr[VIA_REG_EXCN];
}

void via_env_lookup(struct via_vm* vm) {
    struct via_value* env = vm->regs->v_arr[VIA_REG_ENV];
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
    struct via_value* cursor = vm->regs->v_arr[VIA_REG_ENV];
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

    cursor->v_car = via_make_pair(vm, symbol, value);
}

void via_b_env_set(struct via_vm* vm) {
    struct via_value* value = via_pop_arg(vm);
    via_env_set(vm, vm->acc, value);
    vm->ret = value;
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

    value->type = VIA_V_INVALID;
}

static void via_sweep_segment(struct via_segment* segment, uint8_t generation) {
    if (segment->count) {
        for (size_t i = 0; i < segment->num_values; ++i) {
            if (
                (segment->values[i].type != VIA_V_INVALID)
                    && segment->values[i].generation != generation
            ) {
                via_delete_value(&segment->values[i]);
                segment->count--;
            }
        }
    }

    if (segment->next) {
        via_sweep_segment(segment->next, generation);
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
    for (size_t i = 0; i < vm->stack_size; ++i) {
        if (!vm->stack[i] || vm->stack[i]->type == VIA_V_INVALID) {
            // Top of stack reached, stop iterating.
            break;
        }
        via_mark(vm->stack[i], vm->generation);
    }
    via_mark(vm->symbols, vm->generation);

    // Sweeping.

    via_sweep_segment(vm->heap, vm->generation);
}

void via_catch(
    struct via_vm* vm,
    struct via_value* expr,
    struct via_value* handler
) {
    struct via_value* handler_frame = via_make_frame(vm);
    handler_frame->v_arr[VIA_REG_PC]->v_int = VIA_EVAL_PROC; 
    handler_frame->v_arr[VIA_REG_EXPR] = handler;

    vm->regs = via_make_frame(vm);
    vm->regs->v_arr[VIA_REG_EXH] = handler_frame;
    vm->regs->v_arr[VIA_REG_EXPR] = expr;
}

void via_throw(struct via_vm* vm, struct via_value* exception) {
    vm->acc = vm->regs->v_arr[VIA_REG_EXH];
    via_assume_frame(vm);
    vm->regs->v_arr[VIA_REG_EXCN] = exception;
}

void via_default_exception_handler(struct via_vm* vm) {
    via_return_outer(vm, via_to_string(vm, via_exception(vm)));
}

struct via_value* via_run(struct via_vm* vm) {
    struct via_value* val;
    via_int old_pc;
    via_int op;
    
    via_catch(
        vm,
        vm->regs->v_arr[VIA_REG_EXPR],
        via_make_builtin(
            vm,
            via_bind(vm, via_default_exception_handler)
        )
    );
    // Replace the current frame with the catch clause. 
    vm->regs->v_arr[VIA_REG_PARN] = NULL;

process_state:
    op = vm->program[vm->regs->v_arr[VIA_REG_PC]->v_int];
    DPRINTF("PC %04x: ", vm->regs->v_arr[VIA_REG_PC]->v_int);
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
        DPRINTF("CALL %04x\n", op >> 8);
        vm->regs->v_arr[VIA_REG_PC]->v_int = op >> 8;
        DPRINTF("-------------\n");
        goto process_state;
    case VIA_OP_CALLACC:
        DPRINTF("CALLACC (acc = %04x)\n", vm->acc->v_int);
        vm->regs->v_arr[VIA_REG_PC]->v_int = vm->acc->v_int;
        DPRINTF("-------------\n");
        goto process_state;
    case VIA_OP_CALLB:
        DPRINTF("CALLB %04x\n", op >> 8);
        old_pc = vm->regs->v_arr[VIA_REG_PC]->v_int;
        vm->bound[op >> 8](vm);
        if (old_pc != vm->regs->v_arr[VIA_REG_PC]->v_int) {
            DPRINTF("-------------\n");
            // PC was changed from within the bound function, process without
            // advancing.
            goto process_state;
        }
        break;
    case VIA_OP_SET:
        DPRINTF("SET %d\n", op >> 8);
        vm->regs->v_arr[op >> 8] = vm->acc;
        break;
    case VIA_OP_LOAD:
        DPRINTF("LOAD %d\n", op >> 8);
        vm->acc = vm->regs->v_arr[op >> 8];
        break;
    case VIA_OP_SETRET:
        DPRINTF("SETRET\n");
        vm->ret = vm->acc;
        break;
    case VIA_OP_LOADRET:
        DPRINTF("LOADRET\n");
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
        vm->regs->v_arr[VIA_REG_PC]->v_int += (op >> 8);
        DPRINTF("-------------\n");
        break;
    case VIA_OP_SNAP:
        DPRINTF("SNAP %d\n", op >> 8);
        val = via_make_frame(vm);
        vm->regs->v_arr[VIA_REG_PC]->v_int += (op >> 8) + 1;
        vm->regs = val; 
        break;
    case VIA_OP_RETURN:
        DPRINTF("RETURN\n");
        if (!vm->regs->v_arr[VIA_REG_PARN]) {
            return vm->ret;
        }
        vm->acc = vm->regs->v_arr[VIA_REG_PARN];
        via_assume_frame(vm);
        DPRINTF("-------------\n");
        goto process_state;
    case VIA_OP_JMP:
        DPRINTF("JMP %d\n", op >> 8);
        vm->regs->v_arr[VIA_REG_PC]->v_int += (op >> 8);
        DPRINTF("-------------\n");
        goto process_state;
    case VIA_OP_PUSH:
        DPRINTF("PUSH\n");
        via_push(vm, vm->acc);
        break;
    case VIA_OP_POP:
        DPRINTF("POP\n");
        vm->acc = via_pop(vm);
        break;
    case VIA_OP_DROP:
        DPRINTF("DROP\n");
        (void) via_pop(vm);
        break;
    case VIA_OP_PUSHARG:
        DPRINTF("PUSHARG\n");
        via_push_arg(vm, vm->acc);
        break;
    case VIA_OP_POPARG:
        DPRINTF("POPARG\n");
        vm->acc = via_pop_arg(vm);
        break;
    }

    (vm->regs->v_arr[VIA_REG_PC]->v_int)++;
    goto process_state;
}

