#include <testdrive.h>

#include <via/parse.h>
#include <via/vm.h>

#include <math.h>

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
        REQUIRE(result == via_sym(vm, "test-symbol"));
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

    SECTION("Conditional")
        const char* source = "(if #t (if #f 12 34) 56)";
        result = via_parse(vm, source);

        REQUIRE(result);

        expr = via_parse_ctx_program(result);
        vm->regs[VIA_REG_EXPR] = expr->v_car;

        result = via_run(vm);

        REQUIRE(result);
        REQUIRE(result->v_int == 34);
    END_SECTION

    SECTION("Arithmetics")
        const char* source = "(/ (* (- (+ 1.0 1) 1) 6) 2)";
        result = via_parse(vm, source);

        REQUIRE(result);

        expr = via_parse_ctx_program(result);
        vm->regs[VIA_REG_EXPR] = expr->v_car;

        result = via_run(vm);

        REQUIRE(result);
        REQUIRE(result->type == VIA_V_FLOAT);
        REQUIRE(result->v_float == 3.0);
    END_SECTION

    SECTION("Trigonometrics + power of")
        const char* source = "(^ (sin 1.0) (cos 1.0))";
        result = via_parse(vm, source);

        REQUIRE(result);

        expr = via_parse_ctx_program(result);
        vm->regs[VIA_REG_EXPR] = expr->v_car;

        result = via_run(vm);

        REQUIRE(result);
        REQUIRE(result->type == VIA_V_FLOAT);
        REQUIRE(result->v_float == pow(sin(1.0), cos(1.0)));
    END_SECTION

    via_free_vm(vm);
END_FIXTURE

int main(int argc, char** argv) {
    return RUN_TEST(test_programs);
}

