#include <testdrive.h>

#include <via/asm_macros.h>
#include <via/vm.h>

FIXTURE(test_eval, "Eval")
    struct via_vm* vm = via_create_vm();
    REQUIRE(vm);

    struct via_value* result;

    SECTION("Literal value")
        vm->regs[VIA_REG_EXPR] = via_make_value(vm);
        vm->regs[VIA_REG_EXPR]->type = VIA_V_INT;

        result = via_run(vm);

        REQUIRE(result == vm->regs[VIA_REG_EXPR]);
    END_SECTION

    SECTION("Symbol lookup")
        struct via_value* symbol = via_make_value(vm);
        symbol->type = VIA_V_SYMBOL;
        symbol->v_symbol = "test-symbol";
        
        struct via_value* value = via_make_value(vm);

        via_env_set(vm, symbol, value); 
        vm->regs[VIA_REG_EXPR] = symbol;

        result = via_run(vm);

        REQUIRE(result == value);
    END_SECTION

    via_free_vm(vm);
END_FIXTURE

int main(int argc, char** argv) {
    return RUN_TEST(test_eval);
}

