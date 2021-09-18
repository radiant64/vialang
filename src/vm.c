#include <via/vm.h>

#include "eval.h"
#include <via/alloc.h>
#include <via/asm_macros.h>

#include <assert.h>
#include <string.h>

#define SEGMENT_SIZE 32
#define DEFAULT_STACKSIZE 256
#define DEFAULT_BOUND_SIZE 2048
#define DEFAULT_PROGRAM_SIZE 8192
#define DEFAULT_WRITE_START 512

#define STACK_PUSH(vm, value) (vm->stack[vm->stack_top++] = value)
#define STACK_POP(vm) (vm->stack[--vm->stack_top])

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

static struct via_value* via_heap_get(struct via_segment* heap, via_int addr) {
    size_t pos = 0;
    while ((pos += heap->num_values) < addr) {
        heap = heap->next;
        assert(heap); // TODO: Signal error.
    }

    return &heap->values[addr + heap->num_values - pos];
}

static void via_heap_store(
    struct via_segment* heap,
    via_int addr,
    const struct via_value* value
) {
    size_t pos = 0;
    while ((pos += heap->num_values) < addr) {
        heap = heap->next;
        assert(heap); // TODO: Signal error.
    }

    heap->values[addr + heap->num_values - pos] = *value;
}

void via_add_core_routines(struct via_vm* vm) {
    // Add some builtins.
    const via_int lookup_index = vm->num_bound++;
    vm->bound[lookup_index] = via_env_lookup;
    const via_int lookup_form_index = vm->num_bound++;
    vm->bound[lookup_form_index] = via_lookup_form;
    const via_int apply_index = vm->num_bound++;
    vm->bound[apply_index] = via_apply;

    // Copy the native routines into memory.
    memcpy(&vm->program[VIA_EVAL_PROC], via_eval_prg, via_eval_prg_size); 
    memcpy(
        &vm->program[VIA_EVAL_COMPOUND_PROC],
        via_eval_compound_prg,
        via_eval_compound_prg_size
    );

    // Create trampolines for the builtins.
    vm->program[VIA_LOOKUP_PROC] = _CALLB(lookup_index);
    vm->program[VIA_LOOKUP_PROC + 1] = _RETURN();
    vm->program[VIA_LOOKUP_FORM_PROC] = _CALLB(lookup_form_index);
    vm->program[VIA_LOOKUP_FORM_PROC + 1] = _RETURN();
    vm->program[VIA_APPLY_PROC] = _CALLB(apply_index);
    vm->program[VIA_APPLY_PROC + 1] = _RETURN();
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

        vm->regs[VIA_REG_PC] = via_make_value(vm);
        vm->regs[VIA_REG_PC]->type = VIA_V_INT;
        vm->regs[VIA_REG_PC]->v_int = VIA_EVAL_PROC;

        vm->regs[VIA_REG_ENV] = via_make_env(vm);

        via_add_core_routines(vm);
    }

    return vm;

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

struct via_value** via_make_array(struct via_vm* vm, size_t size) {
    return via_calloc(size, sizeof(struct via_value*));
}

struct via_value* via_make_frame(struct via_vm* vm) {
    struct via_value* frame = via_make_value(vm);
    if (!frame) {
        return NULL;
    }

    frame->type = VIA_V_ARRAY;
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
    struct via_value* env = via_make_value(vm);

    env->type = VIA_V_PAIR;
    env->v_car = vm->regs[VIA_REG_ENV];
    
    return env;
}

void via_assume_frame(struct via_vm* vm) {
    assert(vm->acc->type == VIA_V_ARRAY);
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

    return index;
};

void via_apply(struct via_vm* vm) {
    const struct via_value* proc = vm->regs[VIA_REG_PROC];
    const struct via_value* args = vm->regs[VIA_REG_ARGS];

    struct via_value* formals = proc->v_cdr->v_car;
    vm->acc = proc->v_cdr->v_cdr->v_car;
    vm->regs[VIA_REG_ENV] = via_make_env(vm);
    
    while (args) {
        via_env_set(vm, formals->v_car, args->v_car);
        args = args->v_cdr;
        formals = formals->v_cdr;
    }

    vm->regs[VIA_REG_EXPR] = proc->v_car;
    vm->regs[VIA_REG_PC]->v_int = VIA_EVAL_PROC;
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
        cursor->v_cdr = via_make_value(vm);
        cursor = cursor->v_cdr;
        cursor->type = VIA_V_PAIR;
    }

    cursor->v_car = via_make_value(vm);
    cursor->v_car->type = VIA_V_PAIR;
    cursor->v_car->v_car = symbol;
    cursor->v_car->v_cdr = value;
}

void via_lookup_form(struct via_vm* vm) {
}

struct via_value* via_run(struct via_vm* vm) {
    struct via_value* val;
    via_int old_pc;
    via_int op;

process_state:
    op = vm->program[vm->regs[VIA_REG_PC]->v_int];
    switch (op & 0xff) {
    case VIA_OP_NOP:
        break;
    case VIA_OP_CAR:
        vm->acc = vm->acc->v_car;
        break;
    case VIA_OP_CDR:
        vm->acc = vm->acc->v_cdr;
        break;
    case VIA_OP_CALL:
        vm->regs[VIA_REG_PC]->v_int = op >> 8;
        goto process_state;
    case VIA_OP_CALLA:
        vm->regs[VIA_REG_PC]->v_int = vm->acc->v_int;
        goto process_state;
    case VIA_OP_CALLB:
        old_pc = vm->regs[VIA_REG_PC]->v_int;
        vm->bound[op >> 8](vm);
        if (old_pc != vm->regs[VIA_REG_PC]->v_int) {
            // PC was changed from within the bound function, process without
            // advancing.
            goto process_state;
        }
        break;
    case VIA_OP_SETRET:
        vm->ret = vm->acc;
        break;
    case VIA_OP_LOADRET:
        vm->acc = vm->ret;
        break;
    case VIA_OP_SETEXPR:
        vm->regs[VIA_REG_EXPR] = vm->acc;
        break;
    case VIA_OP_LOADEXPR:
        vm->acc = vm->regs[VIA_REG_EXPR];
        break;
    case VIA_OP_SETPROC:
        vm->regs[VIA_REG_PROC] = vm->acc;
        break;
    case VIA_OP_LOADPROC:
        vm->acc = vm->regs[VIA_REG_PROC];
        break;
    case VIA_OP_PAIRP:
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
        val = via_make_value(vm);
        val->type = VIA_V_BOOL;
        val->v_bool = (vm->acc->type == VIA_V_SYMBOL);
        vm->acc = val;
        break;
    case VIA_OP_SKIPZ:
        if (vm->acc->v_bool) {
            break;
        }
        vm->regs[VIA_REG_PC]->v_int += (op >> 8);
        break;
    case VIA_OP_SNAP:
        val = via_make_frame(vm);
        // Always skip ahead one instruction, to maintain consistency with
        // SKIPZ and JMP.
        val->v_array[VIA_REG_PC]->v_int += (op >> 8) + 1;
        STACK_PUSH(vm, val);
        break;
    case VIA_OP_RETURN:
        if (!vm->stack_top) {
            return vm->ret;
        }
        vm->acc = STACK_POP(vm);
        via_assume_frame(vm);
        goto process_state;
    case VIA_OP_JMP:
        vm->regs[VIA_REG_PC]->v_int += (op >> 8);
        goto process_state;
    case VIA_OP_PUSH:
        STACK_PUSH(vm, vm->acc);
        break;
    case VIA_OP_POP:
        vm->acc = STACK_POP(vm);
        break;
    case VIA_OP_DROP:
        (void) STACK_POP(vm);
        break;
    case VIA_OP_PUSHARG:
        val = via_make_value(vm);
        val->type = VIA_V_PAIR;
        val->v_car = vm->acc;
        val->v_cdr = vm->regs[VIA_REG_ARGS];
        vm->regs[VIA_REG_ARGS] = val;
        break;
    case VIA_OP_POPARG:
        vm->acc = vm->regs[VIA_REG_ARGS]->v_car;
        vm->regs[VIA_REG_ARGS] = vm->regs[VIA_REG_ARGS]->v_cdr;
        break;
    }

    (vm->regs[VIA_REG_PC]->v_int)++;
    goto process_state;
}

