#include <via/exceptions.h>

#include <via/vm.h>

#define EXCEPT "exception-type"

via_bool via_is_exception(struct via_vm* vm, const struct via_value* value) {
    return (
        value
            && value->type == VIA_V_PAIR
            && value->v_car == via_sym(vm, EXCEPT)
    );
}

const struct via_value* via_make_exception(
    struct via_vm* vm,
    const char* symbol,
    const char* message
) {
    return via_make_pair(
        vm,
        via_sym(vm, EXCEPT),
        via_make_pair(
            vm,
            via_sym(vm, symbol),
            via_make_pair(
                vm,
                via_make_string(vm, message),
                vm->regs
            )
        )
    );
}

const struct via_value* via_excn_symbol(const struct via_value* exception) {
    return exception->v_cdr->v_car;
}

const struct via_value* via_excn_message(const struct via_value* exception) {
    return exception->v_cdr->v_cdr->v_car;
}

const struct via_value* via_excn_frame(const struct via_value* exception) {
    return exception->v_cdr->v_cdr->v_cdr;
}

const struct via_value* via_except_syntax_error(
    struct via_vm* vm,
    const char* message
) {
    return via_make_exception(vm, "exc-syntax-error", message);
}

const struct via_value* via_except_out_of_memory(
    struct via_vm* vm,
    const char* message
) {
    return via_make_exception(vm, "exc-out-of-memory", message);
}

const struct via_value* via_except_invalid_type(
    struct via_vm* vm,
    const char* message
) {
    return via_make_exception(vm, "exc-invalid-type", message);
}

const struct via_value* via_except_argument_error(
    struct via_vm* vm,
    const char* message
) {
    return via_make_exception(vm, "exc-argument-error", message);
}

const struct via_value* via_except_undefined_value(
    struct via_vm* vm,
    const char* message
) {
    return via_make_exception(vm, "exc-undefined-value", message);
}

const struct via_value* via_except_out_of_bounds(
    struct via_vm* vm,
    const char* message
) {
    return via_make_exception(vm, "exc-out-of-bounds", message);
}

const struct via_value* via_except_runtime_error(
    struct via_vm* vm,
    const char* message
) {
    return via_make_exception(vm, "exc-runtime-error", message);
}

const struct via_value* via_except_io_error(
    struct via_vm* vm,
    const char* message
) {
    return via_make_exception(vm, "exc-io-error", message);
}

const struct via_value* via_except_end_of_file(
    struct via_vm* vm,
    const char* message
) {
    return via_make_exception(vm, "exc-end-of-file", message);
}

