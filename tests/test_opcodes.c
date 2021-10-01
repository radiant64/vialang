#include <testdrive.h>

#include <via/vm.h>

static via_bool builtin_called;
static const via_int test_addr = 8000;

static void test_builtin(struct via_vm* vm) {
    builtin_called = true;
}

static void test_jumping_builtin(struct via_vm* vm) {
    builtin_called = true;
    vm->regs->v_arr[VIA_REG_PC]->v_int = test_addr + 2;
}

FIXTURE(test_opcodes, "Opcodes")
    struct via_vm* vm = via_create_vm();
    REQUIRE(vm);

    vm->regs->v_arr[VIA_REG_PC]->v_int = test_addr;

    struct via_value* result;
    
    // Test values.
    struct via_value* foo = via_make_value(vm);
    struct via_value* bar = via_make_value(vm);

    SECTION("RETURN")
        vm->program[test_addr] = _RETURN();
        result = via_run(vm);

        REQUIRE(result == vm->ret);
        REQUIRE(vm->regs->v_arr[VIA_REG_PC]->v_int == test_addr);
    END_SECTION

    // Ensure the rest of the tests terminate.
    vm->program[test_addr + 1] = _RETURN();

    SECTION("NOP")
        vm->program[test_addr] = _NOP();
        result = via_run(vm);
        
        REQUIRE(vm->regs->v_arr[VIA_REG_PC]->v_int == test_addr + 1);
    END_SECTION

    SECTION("CAR & CDR")
        vm->acc = via_make_value(vm);
        vm->acc->type = VIA_V_PAIR;
        vm->acc->v_car = foo;
        vm->acc->v_cdr = bar;

        SECTION("CAR")
            vm->program[test_addr] = _CAR();
            result = via_run(vm);

            REQUIRE(vm->acc == foo);
        END_SECTION

        SECTION("CDR")
            vm->program[test_addr] = _CDR();
            result = via_run(vm);

            REQUIRE(vm->acc == bar);
        END_SECTION
    END_SECTION

    SECTION("CALL & CALLA")
        vm->program[test_addr + 2] = _RETURN();

        SECTION("CALL")
            vm->program[test_addr] = _CALL(test_addr + 2);
            result = via_run(vm);

            REQUIRE(vm->regs->v_arr[VIA_REG_PC]->v_int == test_addr + 2);
        END_SECTION
        
        SECTION("CALLA")
            vm->acc = via_make_value(vm);
            vm->acc->type = VIA_V_INT;
            vm->acc->v_int = test_addr + 2;
            vm->program[test_addr] = _CALLA();
            result = via_run(vm);

            REQUIRE(vm->regs->v_arr[VIA_REG_PC]->v_int == test_addr + 2);
        END_SECTION
    END_SECTION

    SECTION("CALLB")
        const via_int bound = via_bind(vm, test_builtin);
        const via_int bound_jumping = via_bind(vm, test_jumping_builtin);
        REQUIRE(bound);
        REQUIRE(bound_jumping);

        builtin_called = false;

        SECTION("Regular")
            vm->program[test_addr] = _CALL(bound);
            result = via_run(vm);

            REQUIRE(builtin_called);
            REQUIRE(vm->regs->v_arr[VIA_REG_PC]->v_int == bound + 1);
        END_SECTION

        SECTION("Jumping")
            vm->program[test_addr + 2] = _RETURN();
            vm->program[test_addr] = _CALL(bound_jumping);
            result = via_run(vm);

            REQUIRE(builtin_called);
            REQUIRE(vm->regs->v_arr[VIA_REG_PC]->v_int == test_addr + 2);
        END_SECTION
    END_SECTION

    SECTION("SET")
        vm->acc = foo;
        vm->program[test_addr] = _SET(VIA_REG_EXPR);
        result = via_run(vm);

        REQUIRE(vm->regs->v_arr[VIA_REG_EXPR] == foo);
    END_SECTION

    SECTION("LOAD")
        vm->regs->v_arr[VIA_REG_EXPR] = foo;
        vm->program[test_addr] = _LOAD(VIA_REG_EXPR);
        result = via_run(vm);

        REQUIRE(vm->acc == foo);
    END_SECTION

    SECTION("SETRET")
        vm->acc = foo;
        vm->program[test_addr] = _SETRET();
        result = via_run(vm);

        REQUIRE(vm->ret == foo);
        REQUIRE(result == foo);
    END_SECTION

    SECTION("LOADRET")
        vm->ret = foo;
        vm->program[test_addr] = _LOADRET();
        result = via_run(vm);

        REQUIRE(vm->acc == foo);
    END_SECTION

    SECTION("PAIRP")
        vm->program[test_addr] = _PAIRP();
        vm->acc = foo;

        SECTION("Is pair")
            foo->type = VIA_V_PAIR;
            result = via_run(vm);

            REQUIRE(vm->acc->type == VIA_V_BOOL);
            REQUIRE(vm->acc->v_bool);
        END_SECTION
        
        SECTION("Is not pair")
            foo->type = VIA_V_INT;
            result = via_run(vm);

            REQUIRE(vm->acc->type == VIA_V_BOOL);
            REQUIRE(!vm->acc->v_bool);
        END_SECTION
    END_SECTION

    SECTION("SYMBOLP")
        vm->program[test_addr] = _SYMBOLP();
        vm->acc = foo;

        SECTION("Is symbol")
            foo->type = VIA_V_SYMBOL;
            result = via_run(vm);

            REQUIRE(vm->acc->type == VIA_V_BOOL);
            REQUIRE(vm->acc->v_bool);
        END_SECTION
        
        SECTION("Is not symbol")
            foo->type = VIA_V_INT;
            result = via_run(vm);

            REQUIRE(vm->acc->type == VIA_V_BOOL);
            REQUIRE(!vm->acc->v_bool);
        END_SECTION
    END_SECTION

    SECTION("FORMP")
        vm->program[test_addr] = _FORMP();
        vm->acc = foo;

        SECTION("Is form")
            foo->type = VIA_V_FORM;
            result = via_run(vm);

            REQUIRE(vm->acc->type == VIA_V_BOOL);
            REQUIRE(vm->acc->v_bool);
        END_SECTION
        
        SECTION("Is not form")
            foo->type = VIA_V_INT;
            result = via_run(vm);

            REQUIRE(vm->acc->type == VIA_V_BOOL);
            REQUIRE(!vm->acc->v_bool);
        END_SECTION
    END_SECTION

    SECTION("FRAMEP")
        vm->program[test_addr] = _FRAMEP();
        vm->acc = foo;

        SECTION("Is frame")
            foo->type = VIA_V_FRAME;
            result = via_run(vm);

            REQUIRE(vm->acc->type == VIA_V_BOOL);
            REQUIRE(vm->acc->v_bool);
        END_SECTION
        
        SECTION("Is not frame")
            foo->type = VIA_V_INT;
            result = via_run(vm);

            REQUIRE(vm->acc->type == VIA_V_BOOL);
            REQUIRE(!vm->acc->v_bool);
        END_SECTION
    END_SECTION

    SECTION("SKIPZ")
        vm->program[test_addr + 3] = _RETURN();
        vm->program[test_addr] = _SKIPZ(2);
        vm->acc = foo;

        SECTION("Zero")
            foo->v_int = 0;
            result = via_run(vm);

            REQUIRE(vm->regs->v_arr[VIA_REG_PC]->v_int == test_addr + 3);
        END_SECTION
        
        SECTION("Not zero")
            foo->v_int = 1;
            result = via_run(vm);

            REQUIRE(vm->regs->v_arr[VIA_REG_PC]->v_int == test_addr + 1);
        END_SECTION
    END_SECTION

    SECTION("PUSH & POP")
        vm->acc = foo;
        vm->ret = bar;
        vm->program[test_addr] = _PUSH();
        vm->program[test_addr + 1] = _LOADRET();
        vm->program[test_addr + 2] = _POP();
        vm->program[test_addr + 3] = _RETURN();
        result = via_run(vm);

        REQUIRE(result == bar);
        REQUIRE(vm->acc == foo);
    END_SECTION

    SECTION("SNAP")
        vm->acc = foo;
        vm->regs->v_arr[VIA_REG_EXPR] = bar;
        vm->regs->v_arr[VIA_REG_ARGS] = foo;
        vm->regs->v_arr[VIA_REG_PROC] = foo;
        vm->regs->v_arr[VIA_REG_ENV] = foo;
        vm->regs->v_arr[VIA_REG_EXH] = foo;
        vm->program[test_addr] = _SNAP(3);
        vm->program[test_addr + 1] = _SET(VIA_REG_EXPR);
        vm->program[test_addr + 2] = _RETURN();
        vm->program[test_addr + 3] = _RETURN();
        vm->program[test_addr + 4] = _RETURN();

        result = via_run(vm);

        REQUIRE(vm->regs->v_arr[VIA_REG_PC]->v_int == test_addr + 4);
        REQUIRE(vm->regs->v_arr[VIA_REG_EXPR] == bar);
    END_SECTION

    SECTION("JMP")
        vm->program[test_addr] = _JMP(4);
        vm->program[test_addr + 3] = _RETURN();
        vm->program[test_addr + 5] = _JMP(-3);
        result = via_run(vm);

        REQUIRE(vm->regs->v_arr[VIA_REG_PC]->v_int == test_addr + 3);
    END_SECTION

    SECTION("DROP")
        vm->acc = foo;
        vm->ret = bar;
        vm->program[test_addr] = _PUSH();
        vm->program[test_addr + 1] = _LOADRET();
        vm->program[test_addr + 2] = _DROP();
        vm->program[test_addr + 3] = _RETURN();
        result = via_run(vm);

        REQUIRE(vm->acc == bar);
    END_SECTION

    SECTION("PUSHARG")
        vm->acc = foo;
        vm->program[test_addr] = _PUSHARG();
        result = via_run(vm);

        REQUIRE(vm->regs->v_arr[VIA_REG_ARGS]->type == VIA_V_PAIR);
        REQUIRE(vm->regs->v_arr[VIA_REG_ARGS]->v_car == foo);
        REQUIRE(vm->regs->v_arr[VIA_REG_ARGS]->v_cdr == NULL);
        
        SECTION("POPARG")
            vm->acc = bar;
            vm->regs->v_arr[VIA_REG_PC]->v_int = test_addr;
            vm->program[test_addr] = _POPARG();
            result = via_run(vm);

            REQUIRE(vm->acc == foo);
            REQUIRE(vm->regs->v_arr[VIA_REG_ARGS] == NULL);
        END_SECTION
    END_SECTION

    via_free_vm(vm);
END_FIXTURE

int main(int argc, char** argv) {
    return RUN_TEST(test_opcodes);
}

