#pragma once

#include <via/defs.h>

#ifdef __cplusplus
extern "C" {
#endif

enum via_type {
    VIA_V_INVALID,
    VIA_V_NIL,
    VIA_V_OP,
    VIA_V_INT,
    VIA_V_FLOAT,
    VIA_V_BOOL,
    VIA_V_STRING,
    VIA_V_PAIR,
    VIA_V_ARRAY,
    VIA_V_PROC,
    VIA_V_SYMBOL
};

enum via_op {
    VIA_OP_NOP,
    VIA_OP_CAR,
    VIA_OP_CDR,
    VIA_OP_CALL,
    VIA_OP_CALLA,
    VIA_OP_CALLB,
    VIA_OP_SETRET,
    VIA_OP_LOADRET,
    VIA_OP_SETEXPR,
    VIA_OP_LOADEXPR,
    VIA_OP_SETPROC,
    VIA_OP_LOADPROC,
    VIA_OP_PAIRP,
    VIA_OP_SYMBOLP,
    VIA_OP_SKIPZ,
    VIA_OP_SNAP,
    VIA_OP_RETURN,
    VIA_OP_JMP,
    VIA_OP_PUSH,
    VIA_OP_POP,
    VIA_OP_DROP,
    VIA_OP_PUSHARG,
    VIA_OP_POPARG
};

struct via_value {
    enum via_type type;
    union {
        via_int v_op;
        via_int v_int;
        via_float v_float;
        via_bool v_bool;
        const char* v_string;
        const char* v_symbol;
        struct {
            struct via_value* v_car;
            struct via_value* v_cdr;
        };
        struct {
            via_int v_size;
            struct via_value** v_array;
        };
    };
    uint8_t generation;
};

#ifdef __cplusplus
}
#endif

