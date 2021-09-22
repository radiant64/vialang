#include <testdrive.h>

#include <via/parse.h>
#include <via/vm.h>

FIXTURE(test_programs, "Programs")
    struct via_vm* vm = via_create_vm();
    REQUIRE(vm);

    struct via_value* result;
    struct via_value* expr;

    SECTION("Quoted symbol")
        const char* source = "(quote test-symbol)";
        result = via_parse(vm, source);

        REQUIRE(result);

        expr = via_parse_ctx_program(result);
        vm->regs[VIA_REG_EXPR] = expr->v_car;

        result = via_run(vm);

        REQUIRE(result);
        REQUIRE(result == via_symbol(vm, "test-symbol"));
    END_SECTION
    
    SECTION("Sequence")
        const char* source = "(begin (context) 12)";
        result = via_parse(vm, source);

        REQUIRE(result);

        expr = via_parse_ctx_program(result);
        vm->regs[VIA_REG_EXPR] = expr->v_car;

        result = via_run(vm);

        REQUIRE(result);
        REQUIRE(result->v_int == 12);
    END_SECTION

    via_free_vm(vm);
END_FIXTURE

int main(int argc, char** argv) {
    return RUN_TEST(test_programs);
}

