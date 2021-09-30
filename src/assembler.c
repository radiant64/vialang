#include <via/assembler.h>

#include <via/alloc.h>
#include <via/vm.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_SIZE 16

struct via_program {
    via_opcode* code;
    size_t code_size;

    const char* cursor;
    enum via_assembly_status status;

    via_int target_addr;

    char** labels;
    via_int* label_addrs;
    size_t labels_count;
    size_t labels_cap;
};

static via_bool is_whitespace(char c) {
    return c == ' ' || c == '\t';
}

static struct via_program* via_asm_parse_space(
    struct via_program* program
) {
    while (is_whitespace(*program->cursor)) {
        program->cursor++;
    }

    return program;
}

static struct via_program* via_asm_parse_endl(
    struct via_program* program
) {
    if (!*program->cursor) {
        return program;
    }

    const char c = *program->cursor;
    if (c == '\r') {
        program->cursor += (program->cursor[1] == '\n') ? 2 : 1;
    } else if (c == ';' || c == '\n') {
        program->cursor += 1;
    } else {
        program->status = VIA_ASM_SYNTAX_ERROR;
    }

    return program;
}

static via_int via_asm_local_lookup(
    struct via_vm* vm,
    struct via_program* p,
    const char* label
) {
    for (size_t i = 0; i < p->labels_count; ++i) {
        if (strcmp(label, p->labels[i]) == 0) {
            return p->label_addrs[i] + p->target_addr;
        }
    }
    return via_asm_label_lookup(vm, label);
}

static via_int via_asm_register(const char* name) {
    if (strcmp(name, "!expr") == 0) {
        return VIA_REG_EXPR;
    } else if (strcmp(name, "!args") == 0) {
        return VIA_REG_ARGS;
    } else if (strcmp(name, "!proc") == 0) {
        return VIA_REG_PROC;
    } else if (strcmp(name, "!env") == 0) {
        return VIA_REG_ENV;
    } else if (strcmp(name, "!excn") == 0) {
        return VIA_REG_EXCN;
    } else if (strcmp(name, "!exh") == 0) {
        return VIA_REG_EXH;
    } else if (strcmp(name, "!sptr") == 0) {
        return VIA_REG_SPTR;
    } else if (strcmp(name, "!ctxt") == 0) {
        return VIA_REG_CTXT;
    } else if (strcmp(name, "!parn") == 0) {
        return VIA_REG_PARN;
    }

    return -1;
}

static struct via_program* via_asm_parse_instr(
    struct via_vm* vm,
    struct via_program* p,
    via_bool generate_code
) {
    char op[9];
    char arg[81];
    int len = 0;
    via_bool has_arg = true;
    const char* c = p->cursor;

    static const char const* op_arg_f = "%8[a-zA-Z]%*[ \t]%80[a-zA-Z0-9@!_-]%n";
    static const char const* op_f = "%8[a-zA-Z]%n";
    // Instruction with operand.
    if (!sscanf(c, op_arg_f, op, arg, &len) || len == 0) {
        // Try parsing without operand.
        if (!sscanf(c, op_f, op, &len) || len == 0) {
            p->status = VIA_ASM_SYNTAX_ERROR;
            return p;
        }
        has_arg = false;
    }
    if (c[len] == ':') {
        p->status = VIA_ASM_SYNTAX_ERROR;
        return p;
    }

    if (generate_code) {
        via_opcode* dest = &p->code[p->code_size];
        if (has_arg) {
            // Instruction with operand.
            char* end;
            via_int num_arg = strtol(arg, &end, 10);
            if (arg == end) {
                if (arg[0] == '!') {
                    num_arg = via_asm_register(arg);
                } else {
                    num_arg = via_asm_local_lookup(vm, p, arg);
                }
            }

            if (num_arg == -1) {
                p->status = VIA_ASM_UNKNOWN_SYMBOL;
                return p;
            }

            via_int rel_arg =
                (num_arg - (p->target_addr + p->code_size + 1)) << 8; 
            num_arg = num_arg << 8;

            if (strcmp("call", op) == 0) {
                *dest = VIA_OP_CALL | num_arg;
            } else if (strcmp("callb", op) == 0) {
                *dest = VIA_OP_CALLB | num_arg;
            } else if (strcmp("set", op) == 0) {
                *dest = VIA_OP_SET | num_arg;
            } else if (strcmp("load", op) == 0) {
                *dest = VIA_OP_LOAD | num_arg;
            } else if (strcmp("skipz", op) == 0) {
                *dest = VIA_OP_SKIPZ | ((arg == end) ? rel_arg : num_arg);
            } else if (strcmp("snap", op) == 0) {
                *dest = VIA_OP_SNAP | ((arg == end) ? rel_arg : num_arg);
            } else if (strcmp("jmp", op) == 0) {
                *dest = VIA_OP_JMP | ((arg == end) ? rel_arg : num_arg);
            } else {
                p->status = VIA_ASM_SYNTAX_ERROR;
                return p;
            }
        } else {
            // Instruction without operand.
            if (strcmp("nop", op) == 0) {
                *dest = VIA_OP_NOP;
            } else if (strcmp("car", op) == 0) {
                *dest = VIA_OP_CAR;
            } else if (strcmp("cdr", op) == 0) {
                *dest = VIA_OP_CDR;
            } else if (strcmp("callacc", op) == 0) {
                *dest = VIA_OP_CALLACC;
            } else if (strcmp("setret", op) == 0) {
                *dest = VIA_OP_SETRET;
            } else if (strcmp("loadret", op) == 0) {
                *dest = VIA_OP_LOADRET;
            } else if (strcmp("pairp", op) == 0) {
                *dest = VIA_OP_PAIRP;
            } else if (strcmp("symbolp", op) == 0) {
                *dest = VIA_OP_SYMBOLP;
            } else if (strcmp("formp", op) == 0) {
                *dest = VIA_OP_FORMP;
            } else if (strcmp("framep", op) == 0) {
                *dest = VIA_OP_FRAMEP;
            } else if (strcmp("builtinp", op) == 0) {
                *dest = VIA_OP_BUILTINP;
            } else if (strcmp("return", op) == 0) {
                *dest = VIA_OP_RETURN;
            } else if (strcmp("push", op) == 0) {
                *dest = VIA_OP_PUSH;
            } else if (strcmp("pop", op) == 0) {
                *dest = VIA_OP_POP;
            } else if (strcmp("drop", op) == 0) {
                *dest = VIA_OP_DROP;
            } else if (strcmp("pusharg", op) == 0) {
                *dest = VIA_OP_PUSHARG;
            } else if (strcmp("poparg", op) == 0) {
                *dest = VIA_OP_POPARG;
            } else {
                p->status = VIA_ASM_SYNTAX_ERROR;
                return p;
            }
        }
    }
    p->cursor = &p->cursor[len];
    p->code_size++;
    return p;
}

static via_bool via_add_label(
    struct via_program* p,
    char* label,
    via_int addr
) {
    if (p->labels_count == p->labels_cap) {
        char** new_labels = via_realloc(
            p->labels,
            sizeof(char*) * (p->labels_cap * 2)
        );
        if (!new_labels) {
            return false;
        }
        p->labels = new_labels;
        size_t* new_addrs = via_realloc(
            p->label_addrs,
            sizeof(via_int) * (p->labels_cap * 2)
        );
        if (!new_addrs) {
            return false;
        }
        p->label_addrs = new_addrs;
        p->labels_cap *= 2;
    }

    p->labels[p->labels_count] = label;
    p->label_addrs[p->labels_count++] = addr;
    return true;
}

static struct via_program* via_asm_parse_label(
    struct via_program* p,
    via_bool store_labels
) {
    char buffer[81];
    int len = 0;
    const char* c = p->cursor;

    // Try parsing the line as a label.
    if (!sscanf(c, "%80[a-zA-Z0-9@_-]:%n", buffer, &len) || len == 0) {
        // Not a label, or too long line.
        p->status = VIA_ASM_SYNTAX_ERROR;
        return p;
    }

    if (store_labels) {
        char* label = via_malloc(len + 1);
        if (!label) {
            p->status = VIA_ASM_OUT_OF_MEMORY;
            return p;
        }
        memcpy(label, buffer, len + 1); 

        if (!via_add_label(p, label, p->code_size)) {
            p->status = VIA_ASM_OUT_OF_MEMORY;
            via_free(label);
        }
    }

    p->cursor = &p->cursor[len];
    return p;
}

typedef struct via_program*(*via_asm_pass)(struct via_vm*, struct via_program*);

static struct via_program* via_asm_first_pass(
    struct via_vm* vm,
    struct via_program* p
) {
    while (*p->cursor && p->status == VIA_ASM_SUCCESS) {
        p = via_asm_parse_instr(vm, via_asm_parse_space(p), false);
        if (p->status == VIA_ASM_SYNTAX_ERROR) {
            p->status = VIA_ASM_SUCCESS;
            p = via_asm_parse_label(p, true);
        }
       
        if (p->status == VIA_ASM_SUCCESS) {
            p = via_asm_parse_endl(via_asm_parse_space(p));
        }

        if (p->status != VIA_ASM_SUCCESS) {
            return p;
        }
    }

    p->code = via_malloc(sizeof(via_opcode) * p->code_size);
    if (!p->code) {
        p->status = VIA_ASM_OUT_OF_MEMORY;
        return p;
    }

    p->target_addr = via_asm_reserve_prg(vm, p->code_size);
    p->code_size = 0;

    return p;
}

static struct via_program* via_asm_second_pass(
    struct via_vm* vm,
    struct via_program* p
) {
    while (*p->cursor && p->status == VIA_ASM_SUCCESS) {
        p = via_asm_parse_instr(vm, via_asm_parse_space(p), true);
        if (p->status == VIA_ASM_SYNTAX_ERROR) {
            p->status = VIA_ASM_SUCCESS;
            p = via_asm_parse_label(p, false);
        }
       
        if (p->status == VIA_ASM_SUCCESS) {
            p = via_asm_parse_endl(via_asm_parse_space(p));
        }

        if (p->status != VIA_ASM_SUCCESS) {
            return p;
        }
    }

    return p;
}

struct via_assembly_result via_assemble(struct via_vm* vm, const char* source) {
    struct via_program* program = via_calloc(sizeof(struct via_program), 1);
    if (!program) {
        return (struct via_assembly_result) { VIA_ASM_OUT_OF_MEMORY };
    }

    program->cursor = source;

    program->labels = via_calloc(sizeof(char*), INITIAL_SIZE);
    if (!program->labels) {
        return (struct via_assembly_result) { VIA_ASM_OUT_OF_MEMORY };
    }
    program->label_addrs = via_calloc(sizeof(via_int), INITIAL_SIZE);
    if (!program->label_addrs) {
        program->status = VIA_ASM_OUT_OF_MEMORY;
        goto cleanup_labels;
    }
    program->labels_cap = INITIAL_SIZE;

    static const via_asm_pass asm_pass[] = {
        via_asm_first_pass,
        via_asm_second_pass
    };

    for (int pass = 0; pass < 2; ++pass) {
        program = asm_pass[pass](vm, program);

        if (program->status != VIA_ASM_SUCCESS) {
            goto cleanup_code;
        }

        program->cursor = source;
    }
    
    for (size_t i = 0; i < program->code_size; ++i) {
        vm->program[program->target_addr + i] = program->code[i];
    }

    while (vm->labels_count + program->labels_count > vm->labels_cap) {
        char** labels = via_realloc(
            vm->labels,
            sizeof(char*) * (vm->labels_cap * 2)
        );
        if (!labels) {
            goto cleanup_code;
        }
        vm->labels = labels;
        via_int* addrs = via_realloc(
            vm->label_addrs,
            sizeof(via_int) * (vm->labels_cap * 2)
        );
        if (!addrs) {
            goto cleanup_code;
        }
        vm->label_addrs = addrs;
        vm->labels_cap *= 2;
    }

    for (size_t i = 0, j = vm->labels_count; i < program->labels_count; ++i) {
        if (program->labels[i][0] != '@') {
            vm->labels[j] = program->labels[i];
            vm->label_addrs[j++] =
                program->label_addrs[i] + program->target_addr;
            vm->labels_count++;
        } else {
            via_free(program->labels[i]);
        }
    }

cleanup_code:
    if (program->code) {
        via_free(program->code);
    }

cleanup_label_addrs:
    via_free(program->label_addrs);

cleanup_labels:
    via_free(program->labels);

    struct via_assembly_result result = {
        program->status,
        program->target_addr,
        program->cursor
    };

    via_free(program);

    return result;
}

via_int via_asm_reserve_prg(struct via_vm* vm, via_int size) {
    via_int cursor = vm->write_cursor;
    while (cursor + size > vm->program_cap) {
        via_opcode* new_program = via_realloc(
            vm->program,
            sizeof(via_opcode) * (vm->program_cap * 2)
        );
        if (!new_program) {
            return -1;
        }
        vm->program_cap *= 2;
    }
    vm->write_cursor += size;
    return cursor;
}

via_int via_asm_label_lookup(struct via_vm* vm, const char* label) {
    for (size_t i = 0; i < vm->labels_count; ++i) {
        if (strcmp(label, vm->labels[i]) == 0) {
            return vm->label_addrs[i];
        }
    }
    return -1;
}

