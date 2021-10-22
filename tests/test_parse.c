#include <testdrive.h>

#include <via/parse.h>
#include <via/vm.h>

#include <string.h>

FIXTURE(test_parsing, "Parsing")
    struct via_vm* vm = via_create_vm();
    REQUIRE(vm);

    struct via_value* result;
    struct via_value* expr;

    SECTION("Literals")
        SECTION("int")
            const char* source = "123";
            result = via_parse(vm, source);

            REQUIRE(result);

            expr = via_parse_ctx_program(result); 
            REQUIRE(expr->v_car->type == VIA_V_INT);
            REQUIRE(expr->v_car->v_int == 123);
        END_SECTION
        
        SECTION("Negative int")
            const char* source = "-123";
            result = via_parse(vm, source);

            REQUIRE(result);

            expr = via_parse_ctx_program(result); 
            REQUIRE(expr->v_car->type == VIA_V_INT);
            REQUIRE(expr->v_car->v_int == -123);
        END_SECTION
        
        SECTION("float")
            const char* source = ".013";
            result = via_parse(vm, source);

            REQUIRE(result);

            expr = via_parse_ctx_program(result); 
            REQUIRE(expr->v_car->type == VIA_V_FLOAT);
            REQUIRE(expr->v_car->v_float == .013);
        END_SECTION
        
        SECTION("Negative float")
            const char* source = "-.013";
            result = via_parse(vm, source);

            REQUIRE(result);

            expr = via_parse_ctx_program(result); 
            REQUIRE(expr->v_car->type == VIA_V_FLOAT);
            REQUIRE(expr->v_car->v_float == -.013);
        END_SECTION
        
        SECTION("bool")
            SECTION("True")
                const char* source = "#t";
                result = via_parse(vm, source);

                REQUIRE(result);

                expr = via_parse_ctx_program(result); 
                REQUIRE(expr->v_car->type == VIA_V_BOOL);
                REQUIRE(expr->v_car->v_bool);
            END_SECTION
            
            SECTION("False")
                const char* source = "#f";
                result = via_parse(vm, source);

                REQUIRE(result);

                expr = via_parse_ctx_program(result); 
                REQUIRE(expr->v_car->type == VIA_V_BOOL);
                REQUIRE(!expr->v_car->v_bool);
            END_SECTION
            
            SECTION("Invalid 1")
                const char* source = "#x";
                result = via_parse(vm, source);

                REQUIRE(result);

                expr = via_parse_ctx_program(result); 
                REQUIRE(!expr);
            END_SECTION

            SECTION("Invalid 2")
                const char* source = "#foo";
                result = via_parse(vm, source);

                REQUIRE(result);

                expr = via_parse_ctx_program(result); 
                REQUIRE(!expr);
            END_SECTION
        END_SECTION
        
        SECTION("String")
            SECTION("Simple") 
                const char* source = "\"test\"";
                result = via_parse(vm, source);

                REQUIRE(result);

                expr = via_parse_ctx_program(result); 
                REQUIRE(expr->v_car->type == VIA_V_STRING);
                REQUIRE(strcmp(expr->v_car->v_string, "test") == 0);
            END_SECTION

            SECTION("Escaped") 
                const char* source = "\"test\\ntest\"";
                result = via_parse(vm, source);

                REQUIRE(result);

                expr = via_parse_ctx_program(result); 
                REQUIRE(expr);
                REQUIRE(expr->v_car->type == VIA_V_STRING);
                REQUIRE(strcmp(expr->v_car->v_string, "test\ntest") == 0);
            END_SECTION

            SECTION("Invalid") 
                const char* source = "\"test";
                result = via_parse(vm, source);

                REQUIRE(result);

                expr = via_parse_ctx_program(result); 
                REQUIRE(!expr);
            END_SECTION
        END_SECTION

        SECTION("Symbol")
            const char* source = "test";
            result = via_parse(vm, source);

            REQUIRE(result);

            expr = via_parse_ctx_program(result);
            REQUIRE(expr);
            REQUIRE(expr->v_car == via_sym(vm, "test"));
        END_SECTION
    END_SECTION

    SECTION("Compound")
        SECTION("Simple")
            const char* source = "(test)";
            result = via_parse(vm, source);

            REQUIRE(result);

            expr = via_parse_ctx_program(result);
            REQUIRE(expr);
            REQUIRE(expr->v_car->type == VIA_V_PAIR);
            REQUIRE(expr->v_car->v_car == via_sym(vm, "test"));
            REQUIRE(!expr->v_car->v_cdr);
        END_SECTION
        SECTION("Complex")
            const char* source = "(test (foo bar) baz)";
            result = via_parse(vm, source);

            REQUIRE(result);

            expr = via_parse_ctx_program(result);
            REQUIRE(expr);
            REQUIRE(expr->v_car->type == VIA_V_PAIR);
            REQUIRE(expr->v_car->v_car == via_sym(vm, "test"));
            REQUIRE(expr->v_car->v_cdr->v_car->type == VIA_V_PAIR);
            REQUIRE(expr->v_car->v_cdr->v_car->v_car == via_sym(vm, "foo"));
            REQUIRE(
                expr->v_car->v_cdr->v_car->v_cdr->v_car == via_sym(vm, "bar")
            );
            REQUIRE(expr->v_car->v_cdr->v_cdr->v_car == via_sym(vm, "baz"));
        END_SECTION
    END_SECTION
    
    via_free_vm(vm);
END_FIXTURE

int main(int argc, char** argv) {
    return RUN_TEST(test_parsing);
}

