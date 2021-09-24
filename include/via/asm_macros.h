#pragma once

#define _NOP() VIA_OP_NOP 

#define _CAR() VIA_OP_CAR

#define _CDR() VIA_OP_CDR

#define _CALL(_ADDR) (VIA_OP_CALL | (_ADDR << 8))

#define _CALLA() VIA_OP_CALLA

#define _CALLB(_INDEX) (VIA_OP_CALLB | (_INDEX << 8))

#define _SETRET() VIA_OP_SETRET

#define _LOADRET() VIA_OP_LOADRET

#define _SETEXPR() VIA_OP_SETEXPR

#define _LOADEXPR() VIA_OP_LOADEXPR

#define _SETPROC() VIA_OP_SETPROC

#define _LOADPROC() VIA_OP_LOADPROC

#define _LOADCTXT() VIA_OP_LOADCTXT

#define _SETCTXT() VIA_OP_SETCTXT

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

