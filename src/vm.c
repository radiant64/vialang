#include <via/vm.h>

#include "exception-strings.h"

#include <via/alloc.h>
#include <via/assembler.h>
#include <via/builtin.h>
#include <via/exceptions.h>
#include <via/parse.h>
#include <via/port.h>
#include <via/type-utils.h>

#include <builtin-native.h>
#include <native-via.h>

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if 0
#define DPRINTF(...) do { printf(__VA_ARGS__); } while (0)
#define DDPRINTF(...) do {  } while (0)
#elif 0
#define DPRINTF(...) do { printf(__VA_ARGS__); } while (0)
#define DDPRINTF(...) do { printf(__VA_ARGS__); } while (0)
#else
#define DPRINTF(...) do {  } while (0)
#define DDPRINTF(...) do {  } while (0)
#endif

#define DEFAULT_HEAPSIZE 2048 
#define DEFAULT_STACKSIZE 256
#define DEFAULT_BOUND_SIZE 2048
#define DEFAULT_PROGRAM_SIZE 512
#define DEFAULT_LABELS_CAP 16

static void via_env_set_proc(struct via_vm* vm) {
    const struct via_value* symbol = via_pop(vm);
    const struct via_value* value = via_pop_arg(vm);
    via_env_set(vm, symbol, value);
    vm->ret = value;
}

struct via_assembly_result via_add_core_routines(struct via_vm* vm) {
    vm->program[0] = VIA_OP_RETURN;
    vm->write_cursor = 1;

    // Add some builtins.
    via_bind(vm, "lookup-proc", (via_bindable) via_env_lookup);
    via_bind(vm, "apply-proc", (via_bindable) via_apply);
    via_bind(vm, "form-expand-proc", (via_bindable) via_expand_form);
    via_bind(vm, "assume-proc", (via_bindable) via_assume_frame);
    via_bind(vm, "env-set-proc", (via_bindable) via_env_set_proc);

    // Assemble the native routines.
    return via_assemble(vm, builtin_prg); 
}

struct via_vm* via_create_vm() {
    struct via_vm* vm = via_calloc(1, sizeof(struct via_vm));
    if (vm) {
        vm->heap = via_calloc(DEFAULT_HEAPSIZE, sizeof(struct via_value*));
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

        vm->bound = via_calloc(DEFAULT_BOUND_SIZE, sizeof(via_bindable));
        if (!vm->bound) {
            goto cleanup_label_addrs;
        }
        vm->bound_data = via_calloc(DEFAULT_BOUND_SIZE, sizeof(void*));
        if (!vm->bound_data) {
            goto cleanup_bound;
        }
        vm->bound_cap = DEFAULT_BOUND_SIZE;

        vm->stack = via_calloc(DEFAULT_STACKSIZE, sizeof(struct via_value*));
        if (!vm->stack) {
            goto cleanup_bound_data;
        }
        vm->stack_size = DEFAULT_STACKSIZE;

        vm->regs = NULL;
        vm->regs = (struct via_value*) via_make_frame(vm);
        via_set_pc(vm, (struct via_value*) via_make_int(vm, 0));
        via_set_env(vm, via_make_env(vm, NULL));
        via_set_sptr(vm, (struct via_value*) via_make_int(vm, 0));

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

        const struct via_value* native = via_parse(vm, native_via, NULL);
        if (!via_parse_success(native)) {
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
            const struct via_value* result = via_run_eval(vm); 
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
    via_free((struct via_value**) vm->stack);

cleanup_bound_data:
    via_free(vm->bound);

cleanup_bound:
    via_free(vm->bound);

cleanup_label_addrs:
    via_free(vm->label_addrs);

cleanup_labels:
    via_free(vm->labels);

cleanup_program:
    via_free(vm->program);

cleanup_heap:
    via_free((struct via_value**) vm->heap);

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
        via_free((struct via_value*) value->v_arr);
        break;
    case VIA_V_HANDLE:
        via_file_close(value);
        break;
    }

    via_free(value);
}

void via_free_vm(struct via_vm* vm) {
    via_free((struct via_value**) vm->stack);
    via_free(vm->bound_data);
    via_free(vm->bound);
    via_free(vm->label_addrs);

    for (via_int i = 0; i < vm->labels_count; ++i) {
        via_free(vm->labels[i]);
    }
    via_free(vm->labels);

    via_free(vm->program);

    for (via_int i = 0; i < vm->heap_cap; ++i) {
        if (vm->heap[i]) {
            via_delete_value((struct via_value*) vm->heap[i]);
        }
    }
    via_free((struct via_value**) vm->heap);

    via_free(vm);
}

struct via_value* via_make_value(struct via_vm* vm) {
    size_t i = vm->heap_free;
    for (; i < vm->heap_top + 1 && (vm->heap[i] != NULL); ++i) {
    }

    if (i == vm->heap_cap) {
        const struct via_value** new_heap = via_realloc(
            (struct via_value**) vm->heap,
            sizeof(struct via_value*) * vm->heap_cap * 2
        );
        if (!new_heap) {
            return NULL;
        }
        memset(
            &new_heap[vm->heap_cap],
            0,
            sizeof(struct via_value*) * vm->heap_cap
        );
        vm->heap = new_heap;
        vm->heap_cap *= 2;
    }

    if (i > vm->heap_top) {
        vm->heap_top = i;
    }
    struct via_value* val = via_calloc(1, sizeof(struct via_value));
    vm->heap[i] = val;
    vm->heap_free = i + 1;
    val->type = VIA_V_NIL;
    return val;
}

const struct via_value* via_make_frame(struct via_vm* vm) {
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
    ((struct via_value*) frame->v_arr[VIA_REG_PC])->type = VIA_V_INT;
    if (vm->regs) {
        ((struct via_value*) frame->v_arr[VIA_REG_PC])->v_int =
            via_reg_pc(vm)->v_int;
        for (size_t i = 1; i < VIA_REG_COUNT - 1; ++i) {
            const struct via_value** fval = &frame->v_arr[i];
            *fval = vm->regs->v_arr[i];
        }
    }
    frame->v_arr[VIA_REG_PARN] = vm->regs;

    if (vm->regs) {
        DPRINTF(
            "\t\t\tExpr: %s\n",
            via_to_string(vm, via_reg_expr(vm))->v_string
        );
    }

    return frame;
}

const struct via_value* via_sym(struct via_vm* vm, const char* name) {
    struct via_value* entry;
    const struct via_value* cursor = vm->symbols;
    if (!vm->symbols) {
        entry = (struct via_value*) via_make_string(vm, name);
        ((struct via_value*) entry)->type = VIA_V_SYMBOL;

        vm->symbols = (struct via_value*) via_make_pair(vm, entry, NULL);
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

    entry = (struct via_value*) via_make_string(vm, name);
    entry->type = VIA_V_SYMBOL;

    ((struct via_value*) cursor)->v_cdr = via_make_pair(vm, entry, NULL);

    return cursor->v_cdr->v_car;
}

void via_return_outer(struct via_vm* vm, const struct via_value* value) {
    vm->ret = value;
    vm->regs = (struct via_value*) via_reg_parn(vm);
    via_reg_pc(vm)->v_int = 0;
}

void via_assume_frame(struct via_vm* vm) {
    assert(vm->acc->type == VIA_V_FRAME);
    vm->regs = (struct via_value*) vm->acc;
}

via_int via_bind(struct via_vm* vm, const char* name, via_bindable func) {
    return via_bind_context(vm, name, func, NULL);
};

via_int via_bind_context(
    struct via_vm* vm,
    const char* name,
    via_bindable func,
    void* user_data
) {
    if (vm->bound_count == vm->bound_cap) {
        via_bindable* new_bound = via_realloc(
            vm->bound,
            sizeof(via_bindable) * vm->bound_cap * 2
        );
        if (!new_bound) {
            return -1;
        }
        vm->bound = new_bound;
        void** new_bound_data = via_realloc(
            vm->bound_data,
            sizeof(void*) * vm->bound_cap * 2
        );
        if (!new_bound_data) {
            return -1;
        }
        vm->bound_data = new_bound_data;
        vm->bound_cap *= 2;
    }
    const via_int index = vm->bound_count++;
    vm->bound[index] = func;
    vm->bound_data[index] = user_data;

    char buffer[256];
    snprintf(buffer, 256, "%s:\ncallb %" VIA_FMTId "\nreturn", name, index);
    struct via_assembly_result result = via_assemble(vm, buffer); 

    return result.addr;

}

void via_register_proc(
    struct via_vm* vm,
    const char* symbol,
    const char* asm_label,
    const struct via_value* formals,
    via_bindable func
) {
    via_register_proc_context(vm, symbol, asm_label, formals, func, NULL);
}

void via_register_proc_context(
    struct via_vm* vm,
    const char* symbol,
    const char* asm_label,
    const struct via_value* formals,
    via_bindable func,
    void* user_data
) {
    via_env_set(
        vm,
        via_sym(vm, symbol),
        via_make_proc(
            vm,
            via_make_builtin(
                vm,
                via_bind_context(vm, asm_label, func, user_data)
            ),
            formals,
            via_reg_env(vm)
        )
    );
}

void via_register_native_proc(
    struct via_vm* vm,
    const char* symbol,
    const char* asm_label,
    const struct via_value* formals
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
    const struct via_value* formals,
    via_bindable func
) {
    via_register_form_context(vm, symbol, asm_label, formals, func, NULL);
}

void via_register_form_context(
    struct via_vm* vm,
    const char* symbol,
    const char* asm_label,
    const struct via_value* formals,
    via_bindable func,
    void* user_data
) {
    const struct via_value* form = via_make_form(
        vm,
        formals,
        NULL,
        via_make_builtin(vm, via_bind_context(vm, asm_label, func, user_data))
    );

    via_env_set(vm, via_sym(vm, symbol), form);
}

void via_register_native_form(
    struct via_vm* vm,
    const char* symbol,
    const char* asm_label,
    const struct via_value* formals
) {
    const struct via_value* form = via_make_form(
        vm,
        formals,
        NULL,
        via_make_builtin(vm, via_asm_label_lookup(vm, asm_label))
    );

    via_env_set(vm, via_sym(vm, symbol), form);
}

const struct via_value* via_get(struct via_vm* vm, const char* symbol_name) {
    return via_env_lookup_nothrow(vm, via_sym(vm, symbol_name));
}

void via_push(struct via_vm* vm, const struct via_value* value) {
    const via_int top = via_reg_sptr(vm)->v_int;
    if (top == vm->stack_size) {
        const struct via_value** new_stack = via_realloc(
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

const struct via_value* via_pop(struct via_vm* vm) {
    const struct via_value* val = vm->stack[--(via_reg_sptr(vm)->v_int)];
    vm->stack[via_reg_sptr(vm)->v_int] = NULL;
    return val;
}

void via_push_arg(struct via_vm* vm, const struct via_value* val) {
    via_set_args(vm, via_make_pair(vm, val, via_reg_args(vm)));
}

const struct via_value* via_pop_arg(struct via_vm* vm) {
    if (!via_reg_args(vm)) {
        return NULL;
    }
    const struct via_value* val = via_reg_args(vm)->v_car;
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
    const struct via_value* args = via_reverse_list(vm, via_reg_args(vm));

    via_set_args(vm, args);

    const struct via_value* formals = proc->v_cdr->v_car;
    vm->acc = proc->v_cdr->v_cdr->v_car;
    via_set_env(vm, via_make_env(vm, proc->v_cdr->v_cdr->v_car));
    DPRINTF(
        "Apply: %s (%s)\n",
        via_to_string(vm, proc)->v_string,
        via_to_string(vm, args)->v_string
    );
    DPRINTF("Args: %s\n", via_to_string(vm, via_reg_args(vm))->v_string);
    DPRINTF("Formals: %s\n", via_to_string(vm, formals)->v_string);
    DPRINTF("New application environment: %p\n", via_reg_env(vm));
    DPRINTF("Parent: %p\n", via_reg_env(vm)->v_car);

    while (formals) {
        if (!args) {
            via_env_set(vm, formals->v_car, NULL);
            formals = formals->v_cdr;
            continue;
        } else if (!formals->v_cdr && args->v_cdr) {
            via_env_set(vm, formals->v_car, args);
        } else {
            via_env_set(vm, formals->v_car, args->v_car);
        }
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

    const struct via_value* routine = vm->acc->v_cdr->v_cdr;

    via_set_ctxt(vm, via_make_pair(vm, vm->acc, via_reg_ctxt(vm)));
    via_set_expr(vm, routine);
    via_reg_pc(vm)->v_int = eval_proc;
}

struct via_value* via_reg_pc(struct via_vm* vm) {
    return (struct via_value*) vm->regs->v_arr[VIA_REG_PC];
}

const struct via_value* via_reg_expr(struct via_vm* vm) {
    return vm->regs->v_arr[VIA_REG_EXPR];
}

const struct via_value* via_reg_proc(struct via_vm* vm) {
    return vm->regs->v_arr[VIA_REG_PROC];
}

const struct via_value* via_reg_args(struct via_vm* vm) {
    return vm->regs->v_arr[VIA_REG_ARGS];
}

const struct via_value* via_reg_env(struct via_vm* vm) {
    return vm->regs->v_arr[VIA_REG_ENV];
}

const struct via_value* via_reg_excn(struct via_vm* vm) {
    return vm->regs->v_arr[VIA_REG_EXCN];
}

const struct via_value* via_reg_exh(struct via_vm* vm) {
    return vm->regs->v_arr[VIA_REG_EXH];
}

struct via_value* via_reg_sptr(struct via_vm* vm) {
    return (struct via_value*) vm->regs->v_arr[VIA_REG_SPTR];
}

const struct via_value* via_reg_ctxt(struct via_vm* vm) {
    return vm->regs->v_arr[VIA_REG_CTXT];
}

const struct via_value* via_reg_parn(struct via_vm* vm) {
    return vm->regs->v_arr[VIA_REG_PARN];
}

void via_set_pc(struct via_vm* vm, struct via_value* value) {
    vm->regs->v_arr[VIA_REG_PC] = value;
}

void via_set_expr(struct via_vm* vm, const struct via_value* value) {
    vm->regs->v_arr[VIA_REG_EXPR] = value;
}

void via_set_proc(struct via_vm* vm, const struct via_value* value) {
    vm->regs->v_arr[VIA_REG_PROC] = value;
}

void via_set_args(struct via_vm* vm, const struct via_value* value) {
    vm->regs->v_arr[VIA_REG_ARGS] = value;
}

void via_set_env(struct via_vm* vm, const struct via_value* value) {
    vm->regs->v_arr[VIA_REG_ENV] = value;
}

void via_set_excn(struct via_vm* vm, const struct via_value* value) {
    vm->regs->v_arr[VIA_REG_EXCN] = value;
}

void via_set_exh(struct via_vm* vm, const struct via_value* value) {
    vm->regs->v_arr[VIA_REG_EXH] = value;
}

void via_set_sptr(struct via_vm* vm, struct via_value* value) {
    vm->regs->v_arr[VIA_REG_SPTR] = value;
}

void via_set_ctxt(struct via_vm* vm, const struct via_value* value) {
    vm->regs->v_arr[VIA_REG_CTXT] = value;
}

void via_set_parn(struct via_vm* vm, const struct via_value* value) {
    vm->regs->v_arr[VIA_REG_PARN] = value;
}


const struct via_value* via_env_lookup_nothrow(
    struct via_vm* vm,
    const struct via_value* symbol
) {
    const struct via_value* env = via_reg_env(vm);
    do {
        const struct via_value* cursor = env;
        if (cursor->v_cdr) {
            do {
                cursor = cursor->v_cdr;
                if (cursor->v_car->v_car == symbol) {
                    return cursor->v_car->v_cdr;
                }
            } while (cursor->v_cdr);
        }
        env = env->v_car;
    } while (env);

    struct via_value* value = via_make_value(vm);
    value->type = VIA_V_UNDEFINED;

    return value;
}

void via_env_lookup(struct via_vm* vm) {
    vm->ret = via_env_lookup_nothrow(vm, vm->acc);

    if (vm->ret && vm->ret->type == VIA_V_UNDEFINED) {
        via_throw(vm, via_except_undefined_value(vm, vm->acc->v_symbol));
    }
}

void via_env_set(
    struct via_vm* vm,
    const struct via_value* symbol,
    const struct via_value* value
) {
    const struct via_value* cursor = via_reg_env(vm);
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
        ((struct via_value*) cursor)->v_cdr = via_make_pair(vm, NULL, NULL);
        cursor = cursor->v_cdr;
    }

    DPRINTF(
        "\t\t%s = %s\n",
        via_to_string(vm, symbol)->v_string,
        via_to_string(vm, value)->v_string
    );

    ((struct via_value*) cursor)->v_car = via_make_pair(vm, symbol, value);
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
        via_mark((struct via_value*) value->v_car, generation);
        via_mark((struct via_value*) value->v_cdr, generation);
        break;
    case VIA_V_FRAME:
    case VIA_V_ARRAY:
        for (size_t i = 0; i < value->v_size; ++i) {
            via_mark((struct via_value*) value->v_arr[i], generation);
        }
        break;
    }
}

static void via_sweep(struct via_vm* vm) {
    for (size_t i = 0; i < vm->heap_cap; ++i) {
        if (vm->heap[i] && vm->heap[i]->generation != vm->generation) {
            via_delete_value((struct via_value*) vm->heap[i]);
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
    via_mark((struct via_value*) vm->acc, vm->generation);
    via_mark((struct via_value*) vm->ret, vm->generation);
    for (size_t i = 0; i < VIA_REG_COUNT; ++i) {
        via_mark((struct via_value*) vm->regs, vm->generation);
    }
    for (size_t i = 0; vm->stack[i] && i < vm->stack_size; ++i) {
        via_mark((struct via_value*) vm->stack[i], vm->generation);
    }
    via_mark(vm->symbols, vm->generation);

    // Sweeping.

    via_sweep(vm);
}

void via_catch(
    struct via_vm* vm,
    const struct via_value* expr,
    const struct via_value* handler
) {
    struct via_value* handler_frame = (struct via_value*) via_make_frame(vm);
    ((struct via_value*) handler_frame->v_arr[VIA_REG_PC])->v_int =
        via_asm_label_lookup(
            vm,
            "eval-proc"
        ); 
    handler_frame->v_arr[VIA_REG_EXPR] = handler;
    handler_frame->v_arr[VIA_REG_PARN] = via_reg_parn(vm);

    via_set_exh(vm, handler_frame);
    via_set_expr(vm, expr);
    via_reg_pc(vm)->v_int = via_asm_label_lookup(
        vm,
        "eval-proc"
    ); 
}

void via_throw(struct via_vm* vm, const struct via_value* exception) {
    vm->acc = via_reg_exh(vm);
    via_assume_frame(vm);
    via_set_excn(vm, exception);
}

const struct via_value* via_backtrace(
    struct via_vm* vm,
    const struct via_value* frame
) {
    const struct via_value* trace = NULL;
    while (frame) {
        trace = via_make_pair(vm, frame->v_arr[VIA_REG_EXPR], trace);
        frame = frame->v_arr[VIA_REG_PARN];
    }
    return trace;
}

void via_default_exception_handler(struct via_vm* vm) {
    const struct via_value* excn = via_reg_excn(vm);
    const struct via_value* frame = via_backtrace(vm, via_excn_frame(excn));
    const struct via_value* exc_pair = via_make_pair(
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

    vm->ret = excn;
}

const struct via_value* via_run_eval(struct via_vm* vm) {
    via_reg_pc(vm)->v_int = via_asm_label_lookup(vm, "eval-proc");
    via_catch(
        vm,
        via_reg_expr(vm),
        via_make_builtin(
            vm,
            via_bind(
                vm,
                "default-except-proc",
                (via_bindable) via_default_exception_handler
            )
        )
    );
    // Replace the current frame with the catch clause. 
    via_set_parn(vm, NULL);

    return via_run(vm);
}

const struct via_value* via_run(struct via_vm* vm) {
    const struct via_value* tmp;
    struct via_value* val;
    via_int old_pc;
    via_int op;
    via_int frame = 0;
    void* data;
    
process_state:
    op = vm->program[via_reg_pc(vm)->v_int];
    old_pc = via_reg_pc(vm)->v_int;
    DDPRINTF("PC %04" VIA_FMTIx ": ", via_reg_pc(vm)->v_int);
    switch (op & 0xff) {
    case VIA_OP_NOP:
        DDPRINTF("NOP\n");
        break;
    case VIA_OP_CAR:
        DDPRINTF("CAR\n");
        vm->acc = vm->acc->v_car;
        break;
    case VIA_OP_CDR:
        DDPRINTF("CDR\n");
        vm->acc = vm->acc->v_cdr;
        break;
    case VIA_OP_CALL:
        DDPRINTF("CALL %04" VIA_FMTIx "\n", op >> 8);
        via_reg_pc(vm)->v_int = op >> 8;
        break;
    case VIA_OP_CALLACC:
        DDPRINTF("CALLACC (acc = %04" VIA_FMTIx ")\n", vm->acc->v_int);
        via_reg_pc(vm)->v_int = vm->acc->v_int;
        break;
    case VIA_OP_CALLB:
        DDPRINTF("CALLB %04" VIA_FMTIx "\n", op >> 8);
        data = vm->bound_data[op >> 8];
        // Pass the VM instance as context by default.
        vm->bound[op >> 8](data ? data : vm);
        break;
    case VIA_OP_SET:
        DDPRINTF(
            "SET %" VIA_FMTId " > %s\n",
            op >> 8,
            via_to_string(vm, vm->acc)->v_string
        );
        vm->regs->v_arr[op >> 8] = vm->acc;
        break;
    case VIA_OP_LOAD:
        DDPRINTF(
            "LOAD %" VIA_FMTId " < %s\n",
            op >> 8,
            via_to_string(vm, vm->regs->v_arr[op >> 8])->v_string
        );
        vm->acc = vm->regs->v_arr[op >> 8];
        break;
    case VIA_OP_LOADNIL:
        DDPRINTF("LOADNIL\n");
        vm->acc = NULL;
        break;
    case VIA_OP_SETRET:
        DDPRINTF("SETRET > %s\n", via_to_string(vm, vm->acc)->v_string);
        vm->ret = vm->acc;
        break;
    case VIA_OP_LOADRET:
        DDPRINTF("LOADRET < %s\n", via_to_string(vm, vm->ret)->v_string);
        vm->acc = vm->ret;
        break;
    case VIA_OP_PAIRP:
        DDPRINTF("PAIRP\n");
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
        DDPRINTF("SYMBOLP\n");
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
        DDPRINTF("BUILTINP\n");
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
        DDPRINTF("FORMP\n");
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
        DDPRINTF("FRAMEP\n");
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
        DDPRINTF("SKIPZ\n");
        if (vm->acc && vm->acc->v_int != 0) {
            break;
        }
        via_reg_pc(vm)->v_int += (op >> 8) + 1;
        break;
    case VIA_OP_SNAP:
        DDPRINTF("SNAP %" VIA_FMTId "\n", op >> 8);
        val = (struct via_value*) via_make_frame(vm);
        DPRINTF("\tFrame: %" VIA_FMTId "\n", ++frame);
        DPRINTF(
            "\tExpr: %s\n",
            via_to_string(vm, via_reg_expr(vm))->v_string
        );
        DPRINTF("Environment: %p\n", via_reg_env(vm));
        via_reg_pc(vm)->v_int += (op >> 8) + 1;
        vm->regs = val; 
        break;
    case VIA_OP_RETURN:
        DDPRINTF("RETURN\n");
        if (!via_reg_parn(vm)) {
            return vm->ret;
        }
        vm->acc = via_reg_parn(vm);
        via_assume_frame(vm);
        DPRINTF("\tFrame: %" VIA_FMTId "\n", --frame);
        DPRINTF(
            "\tExpr: %s\n",
            via_to_string(vm, via_reg_expr(vm))->v_string
        );
        DPRINTF(
            "\tRet: %s\n",
            via_to_string(vm, vm->ret)->v_string
        );
        DPRINTF("Environment: %p\n", via_reg_env(vm));
        break;
    case VIA_OP_JMP:
        DDPRINTF("JMP %" VIA_FMTId "\n", op >> 8);
        via_reg_pc(vm)->v_int += (op >> 8) + 1;
        break;
    case VIA_OP_PUSH:
        DDPRINTF("PUSH > %s\n", via_to_string(vm, vm->acc)->v_string);
        via_push(vm, vm->acc);
        break;
    case VIA_OP_POP:
        tmp = via_pop(vm);
        DDPRINTF("POP < %s\n", via_to_string(vm, tmp)->v_string);
        vm->acc = tmp;
        break;
    case VIA_OP_DROP:
        DDPRINTF("DROP\n");
        (void) via_pop(vm);
        break;
    case VIA_OP_PUSHARG:
        DDPRINTF("PUSHARG > %s\n", via_to_string(vm, vm->acc)->v_string);
        via_push_arg(vm, vm->acc);
        break;
    case VIA_OP_POPARG:
        tmp = via_pop_arg(vm);
        DDPRINTF("POPARG < %s\n", via_to_string(vm, tmp)->v_string);
        vm->acc = tmp;
        break;
    case VIA_OP_DEBUG:
        printf(
            "Register %" VIA_FMTId ": %s\n",
            op >> 8,
            via_to_string(vm, vm->regs->v_arr[op >> 8])->v_string
        );
        break;
    }

    if (old_pc == via_reg_pc(vm)->v_int) {
        (via_reg_pc(vm)->v_int)++;
    } else {
        DDPRINTF("-------------\n");
    }
    goto process_state;
}

