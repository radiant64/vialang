#include <via/value.h>

#include <via/asm_macros.h>
#include <via/vm.h>

static const via_int via_eval_prg_impl[] = {
    _LOADEXPR(),

    // Check if the value is a compound expression; if true return the result
    // of a compound evaluation, replacing the current frame.
    _PUSH(),
    _PAIRP(),
    _SKIPZ(2),
        _POP(),
        // Evaluate the form.
        _CALL(VIA_EVAL_COMPOUND_PROC),
    _POP(),

    // Check if the value is a builtin, if true return the result of executing
    // it, replacing the current frame.
    _PUSH(),
    _BUILTINP(),
    _SKIPZ(2),
        _POP(),
        _CALLA(), // Procedure body is a program offset.
    _POP(),

    // Check if the value is a symbol; if true return the lookup result,
    // replacing the current frame.
    _PUSH(),
    _SYMBOLP(),
    _SKIPZ(2),
        _POP(),
        _CALL(VIA_LOOKUP_PROC),
    _POP(),

    // Return the literal result.
    _SETRET(),
    _RETURN()
};
const via_int* via_eval_prg = via_eval_prg_impl;
const size_t via_eval_prg_size = sizeof(via_eval_prg_impl);

static const via_int via_eval_compound_prg_impl[] = {
    _LOADEXPR(),
    _CAR(),

    // Check if the compound expression is a special form. Special forms are
    // identified by their head value, which must be a symbol literal that
    // resolves to a form.
    _PUSH(),
    _SYMBOLP(),
    _SKIPZ(15),
        _POP(),
        _SNAP(1),
            _CALL(VIA_LOOKUP_PROC),
        _LOADRET(),
        _FORMP(),

        // If the symbol resolved to a special form, evaluate the return value
        // of evaluating the form expression.
        _SKIPZ(7),
            _LOADRET(),
            _SNAP(5),
                _LOADEXPR(),
                _PUSHARG(),
                _LOADRET(),
                _SETEXPR(),
                _CALL(VIA_EVAL_PROC),
        _LOADRET(),
        _JMP(2),
    _POP(),

    // Regular form; procedure application.
    // Start out by evaluating the procedure value.
    _SNAP(2),
        _SETEXPR(),
        _CALL(VIA_EVAL_PROC),
    // Set the PROC register to the result, and load the original expression
    // in the accumulator.
    _LOADRET(),
    _SETPROC(),
    _LOADEXPR(),

    // Iterate over the rest of the expression and evaluate the terms as
    // arguments.
    _CDR(),
    _SETEXPR(), 
    _SKIPZ(8), // No more arguments? Branch out of the loop.
        _SNAP(3),
            _CAR(),
            _SETEXPR(),
            _CALL(VIA_EVAL_PROC),
        _LOADRET(),
        _PUSHARG(),
        _LOADEXPR(),
        _JMP(-11), // Loop back to _CDR().

    // Procedure and arguments have been evaluated, calling apply to bind to
    // formals and execute, replacing the current stack frame.
    _CALL(VIA_APPLY_PROC)
};
const via_int* via_eval_compound_prg = via_eval_compound_prg_impl;
const size_t via_eval_compound_prg_size = sizeof(via_eval_compound_prg_impl);

