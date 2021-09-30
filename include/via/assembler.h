#pragma once

#include <via/defs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct via_vm;

enum via_assembly_status {
    VIA_ASM_SUCCESS,
    VIA_ASM_SYNTAX_ERROR,
    VIA_ASM_UNKNOWN_SYMBOL,
    VIA_ASM_OUT_OF_MEMORY
};

struct via_assembly_result {
    enum via_assembly_status status;
    via_int addr;
    const char* err_ptr;
};

struct via_assembly_result via_assemble(struct via_vm* vm, const char* source);

via_int via_asm_reserve_prg(struct via_vm* vm, via_int size);

via_int via_asm_label_lookup(struct via_vm* vm, const char* label);

#ifdef __cplusplus
}
#endif
