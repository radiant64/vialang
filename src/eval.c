#include <via/value.h>

#include <via/asm_macros.h>
#include <via/vm.h>

const via_int via_eval_prg[] = {
    _LOADEXPR(),
    _CAR(),

    // Check if the value is a compound expression; if true return the result
    // of a compound evaluation.
    _PUSH(),
    _PAIRP(),
    _SKIPZ(3),
        _POP(),
        _SETEXPR(),
        // Evaluate the form, replacing the current stack frame.
        _CALL(VIA_EVAL_COMPOUND_PROC),
    _POP(),

    // Check if the value is a symbol; if true return the lookup result,
    // replacing the current stack frame.
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
const size_t via_eval_prg_size = sizeof(via_eval_prg);

const via_int via_eval_compound_prg[] = {
    _LOADEXPR(),
    _CAR(),

    // Check if the compound expression is a special form. Special forms are
    // identified by their head value, which must be a symbol literal that
    // resolves to a form.
    _PUSH(),
    _SYMBOLP(),
    _SKIPZ(6),
        _POP(),
        _SNAP(1),
            _CALL(VIA_LOOKUP_FORM_PROC),
        _LOADRET(),

        // If the symbol resolved to a special form, drop the expression and
        // symbol from the stack and return the result of running the resolved
        // form routine, replacing the current stack frame.
        _SKIPZ(1),
            _CALLA(),
    _DROP(),

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
const size_t via_eval_compound_prg_size = sizeof(via_eval_compound_prg);

