#include <testdrive.h>

#include <via/exceptions.h>
#include <via/parse.h>
#include <via/vm.h>

#include <math.h>

FIXTURE(test_programs, "Programs")
    struct via_vm* vm = via_create_vm();
    REQUIRE(vm);

    const struct via_value* result;
    const struct via_value* expr;

    SECTION("Quoted symbol")
        const char* source = "(quote test-symbol)";
        result = via_parse(vm, source);

        REQUIRE(result);

        expr = via_parse_ctx_program(result);
        via_set_expr(vm, expr->v_car);

        result = via_run_eval(vm);

        REQUIRE(result);
        REQUIRE(result == via_sym(vm, "test-symbol"));
    END_SECTION
    
    SECTION("Sequence")
        const char* source = "(begin (context) 12)";
        result = via_parse(vm, source);

        REQUIRE(result);

        expr = via_parse_ctx_program(result);
        via_set_expr(vm, expr->v_car);

        result = via_run_eval(vm);

        REQUIRE(result);
        REQUIRE(result->v_int == 12);
    END_SECTION

    SECTION("Conditional")
        const char* source = "(if #t (if #f 12 34) 56)";
        result = via_parse(vm, source);

        REQUIRE(result);

        expr = via_parse_ctx_program(result);
        via_set_expr(vm, expr->v_car);

        result = via_run_eval(vm);

        REQUIRE(result);
        REQUIRE(result->v_int == 34);
    END_SECTION

    SECTION("Arithmetics")
        const char* source = "(/ (* (- (+ 1.0 1) 1) 6) 2)";
        result = via_parse(vm, source);

        REQUIRE(result);

        expr = via_parse_ctx_program(result);
        via_set_expr(vm, expr->v_car);

        result = via_run_eval(vm);

        REQUIRE(result);
        REQUIRE(result->type == VIA_V_FLOAT);
        REQUIRE(result->v_float == 3.0);
    END_SECTION

    SECTION("Trigonometrics + power of")
        const char* source = "(^ (sin 1.0) (cos 1.0))";
        result = via_parse(vm, source);

        REQUIRE(result);

        expr = via_parse_ctx_program(result);
        via_set_expr(vm, expr->v_car);

        result = via_run_eval(vm);

        REQUIRE(result);
        REQUIRE(result->type == VIA_V_FLOAT);
        REQUIRE(result->v_float == pow(sin(1.0), cos(1.0)));
    END_SECTION

    SECTION("Factorial")
        const char* source =
            "(begin"
            " (set! factorial"
            "       (lambda (n)"
            "               (begin"
            "                (set! fact-impl"
            "                      (lambda (n acc)"
            "                              (if (= n 1)"
            "                                  acc"
            "                                  (fact-impl (- n 1) (* acc n)))))"
            "                (fact-impl n 1))))"
            " (factorial 5))";
        result = via_parse(vm, source);

        REQUIRE(result);

        expr = via_parse_ctx_program(result);
        via_set_expr(vm, expr->v_car);

        result = via_run_eval(vm);

        REQUIRE(result);
        REQUIRE(result->v_int == 120);
    END_SECTION

    SECTION("Arithmetics type error")
        const char* source = "(+ 43 \"test\")";

        result = via_parse(vm, source);

        REQUIRE(result);

        expr = via_parse_ctx_program(result);
        via_set_expr(vm, expr->v_car);

        result = via_run_eval(vm);

        REQUIRE(result);
        REQUIRE(via_is_exception(vm, result));
    END_SECTION

    SECTION("And/or")
        const char* source = "(or (and #f (+)) #t)";

        result = via_parse(vm, source);

        REQUIRE(result);

        expr = via_parse_ctx_program(result);
        via_set_expr(vm, expr->v_car);

        result = via_run_eval(vm);

        REQUIRE(result);
        REQUIRE(result->type == VIA_V_BOOL);
        REQUIRE(result->v_bool);
    END_SECTION

    SECTION("Bundled native forms and procedures")
        const char* source = "(for-each (cddr (list 1 2 3 4 5)) num "
                                       "(begin (display num) (+ num 1)))";
        result = via_parse(vm, source);

        REQUIRE(result);

        expr = via_parse_ctx_program(result);
        via_set_expr(vm, expr->v_car);

        result = via_run_eval(vm);

        REQUIRE(result);
        REQUIRE(result->type == VIA_V_INT);
        REQUIRE(result->v_int == 6);
    END_SECTION

    SECTION("Tail calls")
        const char* source =
        "(begin"
        "  (set-proc! iterate (num)"
        "             (if (= num 0)"
        "                 0"
        "                 (iterate (- num 1))))"
        "  (iterate 1000))";

        result = via_parse(vm, source);

        REQUIRE(result);

        expr = via_parse_ctx_program(result);
        via_set_expr(vm, expr->v_car);

        result = via_run_eval(vm);

        REQUIRE(result);
        REQUIRE(result->type == VIA_V_INT);
        REQUIRE(result->v_int == 0);
    END_SECTION

    SECTION("Numeric type conversions")
        SECTION("Integers")
            const char* source = "(list (int 1.0) (int #t) (int \"0x01\"))";
            result = via_parse(vm, source);

            REQUIRE(result);

            expr = via_parse_ctx_program(result);
            via_set_expr(vm, expr->v_car);

            result = via_run_eval(vm);

            REQUIRE(result);
            REQUIRE(result->type == VIA_V_PAIR);
            REQUIRE(result->v_car->type == VIA_V_INT);
            REQUIRE(result->v_car->v_int == 1);
            REQUIRE(result->v_cdr->v_car->type == VIA_V_INT);
            REQUIRE(result->v_cdr->v_car->v_int == 1);
            REQUIRE(result->v_cdr->v_cdr->v_car->type == VIA_V_INT);
            REQUIRE(result->v_cdr->v_cdr->v_car->v_int == 1);
        END_SECTION
    END_SECTION

    SECTION("'let' syntax form")
        const char* source =
        "(let ((x 5) (y 10))"
        "  (* x y))";
        result = via_parse(vm, source);

        REQUIRE(result);

        expr = via_parse_ctx_program(result);
        via_set_expr(vm, expr->v_car);

        result = via_run_eval(vm);

        REQUIRE(result);
        REQUIRE(result->type == VIA_V_INT);
        REQUIRE(result->v_int == 50);
    END_SECTION

    via_free_vm(vm);
END_FIXTURE

int main(int argc, char** argv) {
    return RUN_TEST(test_programs);
}

