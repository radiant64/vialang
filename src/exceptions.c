#include <via/exceptions.h>

#include <via/vm.h>

struct via_value* via_except_syntax_error(
    struct via_vm* vm,
    const char* message
) {
    return via_make_pair(
        vm,
        via_sym(vm, "exc-syntax-error"),
        via_make_string(vm, message)
    );
}

struct via_value* via_except_out_of_memory(
    struct via_vm* vm,
    const char* message
) {
    return via_make_pair(
        vm,
        via_sym(vm, "exc-out-of-memory"),
        via_make_string(vm, message)
    );
}

struct via_value* via_except_invalid_type(
    struct via_vm* vm,
    const char* message
) {
    return via_make_pair(
        vm,
        via_sym(vm, "exc-invalid-type"),
        via_make_string(vm, message)
    );
}

struct via_value* via_except_argument_error(
    struct via_vm* vm,
    const char* message
) {
    return via_make_pair(
        vm,
        via_sym(vm, "exc-argument-error"),
        via_make_string(vm, message)
    );
}

struct via_value* via_except_undefined_value(
    struct via_vm* vm,
    const char* message
) {
    return via_make_pair(
        vm,
        via_sym(vm, "exc-undefined-value"),
        via_make_string(vm, message)
    );
}

struct via_value* via_except_out_of_bounds(
    struct via_vm* vm,
    const char* message
) {
    return via_make_pair(
        vm,
        via_sym(vm, "exc-out-of-bounds"),
        via_make_string(vm, message)
    );
}

