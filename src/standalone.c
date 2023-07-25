#include <via/exceptions.h>
#include <via/parse.h>
#include <via/type-utils.h>
#include <via/vm.h>

#include <stdio.h>

#define EVAL_FILE_FUNC\
    "(let ((@main-file (lambda () \"%1$s\"))) (include-file \"%1$s\"))"

void print_usage(const char* binary) {
    fprintf(stdout, "Usage:\n\n\t%s [SCRIPTFILE]\n\n", binary);
    fprintf(
        stdout,
        "Arguments:\n\nSCRIPTFILE\tScript to run (enters batch mode).\n"
    );
}

int dispatch_execution(int argc, char** argv) {
    struct via_vm* vm = via_create_vm();

    char buf[1024];
    switch (argc) {
    case 1:
        fprintf(stdout, "Via " VIA_VERSION "\n\n");
        via_set_expr(
            vm,
            via_parse_ctx_program(via_parse(vm, "(repl)", NULL))->v_car
        );
    break;
    case 2:
        snprintf(buf, 1023, EVAL_FILE_FUNC, argv[1]);
        via_set_expr(
            vm,
            via_parse_ctx_program(via_parse(vm, buf, argv[1]))->v_car
        );
        break;
    default:
        print_usage(argv[0]);
        return -1;
    }
    
    const struct via_value* result = via_run_eval(vm);

    if (!result) {
        return 0;
    }

    if (via_is_exception(vm, result)) {
        fprintf(
            stderr,
            "%s: %s\n",
            via_to_string(vm, via_excn_symbol(via_reg_excn(vm)))->v_string,
            via_to_string(vm, via_excn_message(via_reg_excn(vm)))->v_string
        );
        return -2;
    }

    switch (result->type) {
    case VIA_V_INT:
        return result->v_int;
    case VIA_V_BOOL:
        return result->v_bool ? 0 : 1;
    case VIA_V_FLOAT:
        return (int) result->v_float;
    default:
        return 0;
    }
}

int main(int argc, char** argv) {
    return dispatch_execution(argc, argv);
}

