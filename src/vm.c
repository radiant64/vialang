#include <via/vm.h>

#include "eval.h"
#include <via/alloc.h>
#include <via/asm_macros.h>

#include <assert.h>
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
#define DEFAULT_PROGRAM_SIZE 8192
#define DEFAULT_WRITE_START 512

#define STACK_PUSH(vm, value)\
    (vm->stack[vm->regs[VIA_REG_SPTR]->v_int++] = value)
#define STACK_POP(vm) (vm->stack[--(vm->regs[VIA_REG_SPTR]->v_int)])

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

void via_add_core_routines(struct via_vm* vm) {
    // Add some builtins.
    const via_int lookup_index = vm->num_bound++;
    vm->bound[lookup_index] = via_env_lookup;
    const via_int apply_index = vm->num_bound++;
    vm->bound[apply_index] = via_b_apply;
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

static void via_add_core_forms(struct via_vm* vm) {
    via_register_form(vm, "quote", via_b_quote);
    via_register_form(vm, "begin", via_b_begin);
    via_register_form(vm, "yield", via_b_yield);
}

static void via_add_core_procedures(struct via_vm* vm) {
    via_register_proc(vm, "context", NULL, via_b_context);
}

struct via_vm* via_create_vm() {
    struct via_vm* vm = via_calloc(1, sizeof(struct via_vm));
    if (vm) {
        vm->heap = via_create_segment(SEGMENT_SIZE);
        if (!vm->heap) {
            goto cleanup_vm;
        }

        vm->program = via_calloc(DEFAULT_PROGRAM_SIZE, sizeof(via_int));
        if (!vm->program) {
            goto cleanup_heap;
        }
        vm->write_cursor = 0x200;
        vm->program_cap = DEFAULT_PROGRAM_SIZE;

        vm->bound =
            via_calloc(DEFAULT_BOUND_SIZE, sizeof(void(*)(struct via_vm*)));
        if (!vm->bound) {
            goto cleanup_program;
        }
        vm->bound_cap = DEFAULT_BOUND_SIZE;

        vm->stack = via_calloc(DEFAULT_STACKSIZE, sizeof(struct via_value*));
        if (!vm->stack) {
            goto cleanup_bound;
        }
        vm->stack_size = DEFAULT_STACKSIZE;

        vm->frames = via_calloc(DEFAULT_STACKSIZE, sizeof(struct via_value*));
        if (!vm->frames) {
            goto cleanup_stack;
        }
        vm->frames_size = DEFAULT_STACKSIZE;

        vm->regs[VIA_REG_PC] = via_make_int(vm, VIA_EVAL_PROC);

        vm->regs[VIA_REG_ENV] = via_make_env(vm);
        
        vm->regs[VIA_REG_SPTR] = via_make_int(vm, 0);

        via_add_core_routines(vm);
        via_add_core_forms(vm);
        via_add_core_procedures(vm);
    }

    return vm;

cleanup_stack:
    via_free(vm->stack);

cleanup_bound:
    via_free(vm->bound);

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

static struct via_value* via_make_proc(
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
    frame->v_array = via_make_array(vm, VIA_REG_COUNT);
    if (!frame->v_array) {
        return NULL;
    }
    frame->v_size = VIA_REG_COUNT;

    frame->v_array[VIA_REG_PC] = via_make_value(vm);
    frame->v_array[VIA_REG_PC]->type = VIA_V_INT;
    frame->v_array[VIA_REG_PC]->v_int = vm->regs[VIA_REG_PC]->v_int;
    for (size_t i = 1; i < VIA_REG_COUNT; ++i) {
        struct via_value** fval = &frame->v_array[i];
        *fval = vm->regs[i];
    }

    return frame;
}

struct via_value* via_make_env(struct via_vm* vm) {
    return via_make_pair(vm, vm->regs[VIA_REG_ENV], NULL);
}

struct via_value* via_symbol(struct via_vm* vm, const char* name) {
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

void via_assume_frame(struct via_vm* vm) {
    assert(vm->acc->type == VIA_V_FRAME);
    struct via_value** frame_regs = vm->acc->v_array;

    for (size_t i = 0; i < VIA_REG_COUNT; ++i) {
        vm->regs[i] = frame_regs[i];
    }
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
        via_symbol(vm, symbol),
        via_make_proc(
            vm,
            via_make_builtin(vm, via_bind(vm, func)),
            formals,
            vm->regs[VIA_REG_ENV]
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

    via_env_set(vm, via_symbol(vm, symbol), form);
}

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

void via_b_context(struct via_vm* vm) {
    vm->ret = via_context(vm);
}

void via_b_apply(struct via_vm* vm) {
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

struct via_value* via_get(struct via_vm* vm, const char* symbol_name) {
    struct via_value* tmp_acc = vm->acc;
    struct via_value* tmp_ret = vm->ret;

    vm->acc = via_symbol(vm, symbol_name);
    via_env_lookup(vm);
    struct via_value* value = vm->ret;

    vm->acc = tmp_acc;
    vm->ret = tmp_ret;

    return value;
}

void via_push_arg(struct via_vm* vm, struct via_value* val) {
    vm->regs[VIA_REG_ARGS] = via_make_pair(vm, val, vm->regs[VIA_REG_ARGS]);
}

struct via_value* via_pop_arg(struct via_vm* vm) {
    struct via_value* val = vm->regs[VIA_REG_ARGS]->v_car;
    vm->regs[VIA_REG_ARGS] = vm->regs[VIA_REG_ARGS]->v_cdr;
    return val; 
}

struct via_value* via_context(struct via_vm* vm) {
    return vm->regs[VIA_REG_CTXT];
}

void via_env_lookup(struct via_vm* vm) {
    struct via_value* env = vm->regs[VIA_REG_ENV];
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
    struct via_value* cursor = vm->regs[VIA_REG_ENV];
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

struct via_value* via_run(struct via_vm* vm) {
    struct via_value* val;
    via_int old_pc;
    via_int op;

process_state:
    op = vm->program[vm->regs[VIA_REG_PC]->v_int];
    DPRINTF("PC %04x: ", vm->regs[VIA_REG_PC]->v_int);
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
        vm->regs[VIA_REG_PC]->v_int = op >> 8;
        DPRINTF("-------------\n");
        goto process_state;
    case VIA_OP_CALLA:
        DPRINTF("CALLA (acc = %04x)\n", vm->acc->v_int);
        vm->regs[VIA_REG_PC]->v_int = vm->acc->v_int;
        DPRINTF("-------------\n");
        goto process_state;
    case VIA_OP_CALLB:
        DPRINTF("CALLB %04x\n", op >> 8);
        old_pc = vm->regs[VIA_REG_PC]->v_int;
        vm->bound[op >> 8](vm);
        if (old_pc != vm->regs[VIA_REG_PC]->v_int) {
            DPRINTF("-------------\n");
            // PC was changed from within the bound function, process without
            // advancing.
            goto process_state;
        }
        break;
    case VIA_OP_SETRET:
        DPRINTF("SETRET\n");
        vm->ret = vm->acc;
        break;
    case VIA_OP_LOADRET:
        DPRINTF("LOADRET\n");
        vm->acc = vm->ret;
        break;
    case VIA_OP_SETEXPR:
        DPRINTF("SETEXPR\n");
        vm->regs[VIA_REG_EXPR] = vm->acc;
        break;
    case VIA_OP_LOADEXPR:
        DPRINTF("LOADEXPR\n");
        vm->acc = vm->regs[VIA_REG_EXPR];
        break;
    case VIA_OP_SETPROC:
        DPRINTF("SETPROC\n");
        vm->regs[VIA_REG_PROC] = vm->acc;
        break;
    case VIA_OP_LOADPROC:
        DPRINTF("LOADPROC\n");
        vm->acc = vm->regs[VIA_REG_PROC];
        break;
    case VIA_OP_SETCTXT:
        DPRINTF("SETCTXT\n");
        vm->regs[VIA_REG_CTXT] = vm->acc;
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
        vm->regs[VIA_REG_PC]->v_int += (op >> 8);
        DPRINTF("-------------\n");
        break;
    case VIA_OP_SNAP:
        DPRINTF("SNAP %d\n", op >> 8);
        val = via_make_frame(vm);
        // Always skip ahead one instruction, to maintain consistency with
        // SKIPZ and JMP.
        val->v_array[VIA_REG_PC]->v_int += (op >> 8) + 1;
        vm->frames[vm->frames_top++] = val;
        break;
    case VIA_OP_RETURN:
        DPRINTF("RETURN\n");
        if (!vm->frames_top) {
            return vm->ret;
        }
        vm->acc = vm->frames[--vm->frames_top];
        via_assume_frame(vm);
        DPRINTF("-------------\n");
        goto process_state;
    case VIA_OP_JMP:
        DPRINTF("JMP %d\n", op >> 8);
        vm->regs[VIA_REG_PC]->v_int += (op >> 8);
        DPRINTF("-------------\n");
        goto process_state;
    case VIA_OP_PUSH:
        DPRINTF("PUSH\n");
        STACK_PUSH(vm, vm->acc);
        break;
    case VIA_OP_POP:
        DPRINTF("POP\n");
        vm->acc = STACK_POP(vm);
        break;
    case VIA_OP_DROP:
        DPRINTF("DROP\n");
        (void) STACK_POP(vm);
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

    (vm->regs[VIA_REG_PC]->v_int)++;
    goto process_state;
}

