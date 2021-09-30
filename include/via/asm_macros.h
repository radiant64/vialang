#pragma once

// Labels are defined by setting the highest bit of the operand to a NOP opcode,
// while leaving the rest of the operand bits cleared. The label name is read as
// a const char pointer from the next value in the unassembled program. Both
// values will be omitted from the assembled output.
#define _LABEL(NAME)\
    ((0x80 << (sizeof(uintptr_t) - 1)) | VIA_OP_NOP),\
    ((uintptr_t) NAME)

// Similarly, a label address can be used as an operand to a non-NOP opcode by
// setting the highest bit of its operand, and leaving the rest of the bits
// unset. The label name is again read as a const char pointer from the next
// value in the unassembled program, and the assembled output will be the opcode
// with the adress of the label as the operand.
#define _(OP, NAME)\
    ((0x80 << (sizeof(uintptr_t) - 1)) | OP),\
    ((uintptr_t) NAME)

// A local label won't be visible outside of the current assembly, but otherwise
// works similarly to regular labels as described above. They are denoted by the
// top two bits of the NOP operand being set, and will shadow any global label
// definition of the same name.
#define _L_LABEL(NAME)\
    ((0xc0 << (sizeof(uintptr_t) - 1)) | VIA_OP_NOP),\
    ((uintptr_t) NAME)

#define _NOP() VIA_OP_NOP 

#define _CAR() VIA_OP_CAR

#define _CDR() VIA_OP_CDR

#define _CALL(_ADDR) (VIA_OP_CALL | (_ADDR << 8))

#define _CALLA() VIA_OP_CALLACC

#define _CALLB(_INDEX) (VIA_OP_CALLB | (_INDEX << 8))

#define _SET(REG) (VIA_OP_SET | (REG << 8))

#define _LOAD(REG) (VIA_OP_LOAD | (REG << 8))

#define _SETRET() VIA_OP_SETRET

#define _LOADRET() VIA_OP_LOADRET

#define _PAIRP() VIA_OP_PAIRP

#define _SYMBOLP() VIA_OP_SYMBOLP

#define _FORMP() VIA_OP_FORMP

#define _FRAMEP() VIA_OP_FRAMEP

#define _BUILTINP() VIA_OP_BUILTINP

#define _SKIPZ(_OFFSET) (VIA_OP_SKIPZ | (_OFFSET << 8))

#define _SNAP(_OFFSET) (VIA_OP_SNAP | (_OFFSET << 8))

#define _RETURN() VIA_OP_RETURN

#define _JMP(_ADDR) (VIA_OP_JMP | (_ADDR << 8))

#define _PUSH() VIA_OP_PUSH

#define _POP() VIA_OP_POP

#define _DROP() VIA_OP_DROP

#define _PUSHARG() VIA_OP_PUSHARG

#define _POPARG() VIA_OP_POPARG

