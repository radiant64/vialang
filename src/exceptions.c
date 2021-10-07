#include <via/exceptions.h>

#include <via/vm.h>

struct via_value* via_except_type_error(
    struct via_vm* vm,
    const char* message
) {
    return via_make_pair(
        vm,
        via_sym(vm, "exc-type-error"),
        via_make_string(vm, message)
    );
}

