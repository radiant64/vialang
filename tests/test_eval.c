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

    SECTION("Procedure application")
        struct via_value* symbol = via_make_value(vm);
        symbol->type = VIA_V_SYMBOL;
        symbol->v_symbol = "value";

        struct via_value* formals = via_make_value(vm);
        formals->type = VIA_V_PAIR;
        formals->v_car = symbol;

        struct via_value* proc = via_make_value(vm);
        proc->type = VIA_V_PROC;

        // Body
        proc->v_car = symbol;

        // Formals
        proc->v_cdr = via_make_value(vm);
        proc->v_cdr->type = VIA_V_PAIR;
        proc->v_cdr->v_car = formals;

        // Env
        proc->v_cdr->v_cdr = via_make_value(vm);
        proc->v_cdr->v_cdr->type = VIA_V_PAIR;
        proc->v_cdr->v_cdr->v_car = vm->regs[VIA_REG_ENV];

        // Compound expression
        struct via_value* compound = via_make_value(vm);
        compound->type = VIA_V_PAIR;
        compound->v_car = proc;
        compound->v_cdr = via_make_value(vm);
        compound->v_cdr->type = VIA_V_PAIR;
        compound->v_cdr->v_car = via_make_value(vm);
        compound->v_cdr->v_car->type = VIA_V_INT;
        compound->v_cdr->v_car->v_int = 123;

        vm->regs[VIA_REG_EXPR] = compound;

        result = via_run(vm);

        REQUIRE(result->type == VIA_V_INT);
        REQUIRE(result->v_int == 123);
    END_SECTION

    via_free_vm(vm);
END_FIXTURE

int main(int argc, char** argv) {
    return RUN_TEST(test_eval);
}

