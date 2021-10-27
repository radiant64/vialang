#include <testdrive.h>

#include <via/vm.h>

static via_bool builtin_called;
static const via_int test_addr = 2000;

static void test_builtin(struct via_vm* vm) {
    builtin_called = true;
}

static void test_jumping_builtin(struct via_vm* vm) {
    builtin_called = true;
    via_reg_pc(vm)->v_int = test_addr + 2;
}

FIXTURE(test_opcodes, "Opcodes")
    struct via_vm* vm = via_create_vm();
    REQUIRE(vm);

    via_reg_pc(vm)->v_int = test_addr;

    const struct via_value* result = NULL;
    
    // Test values.
    struct via_value* foo = via_make_value(vm);
    struct via_value* bar = via_make_value(vm);

    SECTION("RETURN")
        vm->program[test_addr] = VIA_OP_RETURN;
        result = via_run(vm);

        REQUIRE(result == vm->ret);
        REQUIRE(via_reg_pc(vm)->v_int == test_addr);
    END_SECTION

    // Ensure the rest of the tests terminate.
    vm->program[test_addr + 1] = VIA_OP_RETURN;

    SECTION("NOP")
        vm->program[test_addr] = VIA_OP_NOP;
        result = via_run(vm);
        
        REQUIRE(via_reg_pc(vm)->v_int == test_addr + 1);
    END_SECTION

    SECTION("CAR & CDR")
        vm->acc = via_make_pair(vm, foo, bar);

        SECTION("CAR")
            vm->program[test_addr] = VIA_OP_CAR;
            result = via_run(vm);

            REQUIRE(vm->acc == foo);
        END_SECTION

        SECTION("CDR")
            vm->program[test_addr] = VIA_OP_CDR;
            result = via_run(vm);

            REQUIRE(vm->acc == bar);
        END_SECTION
    END_SECTION

    SECTION("CALL & CALLACC")
        vm->program[test_addr + 2] = VIA_OP_RETURN;

        SECTION("CALL")
            vm->program[test_addr] = VIA_OP_CALL | ((test_addr + 2) << 8);
            result = via_run(vm);

            REQUIRE(via_reg_pc(vm)->v_int == test_addr + 2);
        END_SECTION
        
        SECTION("CALLACC")
            vm->acc = via_make_int(vm, test_addr + 2);
            vm->program[test_addr] = VIA_OP_CALLACC;
            result = via_run(vm);

            REQUIRE(via_reg_pc(vm)->v_int == test_addr + 2);
        END_SECTION
    END_SECTION

    SECTION("CALLB")
        const via_int bound = via_bind(
            vm,
            "test-proc",
            (via_bindable) test_builtin
        );
        const via_int bound_jumping = via_bind(
            vm,
            "jumping-proc",
            (via_bindable) test_jumping_builtin
        );
        REQUIRE(bound);
        REQUIRE(bound_jumping);

        builtin_called = false;

        SECTION("Regular")
            vm->program[test_addr] = VIA_OP_CALL | (bound << 8);
            result = via_run(vm);

            REQUIRE(builtin_called);
            REQUIRE(via_reg_pc(vm)->v_int == bound + 1);
        END_SECTION

        SECTION("Jumping")
            vm->program[test_addr + 2] = VIA_OP_RETURN;
            vm->program[test_addr] = VIA_OP_CALL | (bound_jumping << 8);
            result = via_run(vm);

            REQUIRE(builtin_called);
            REQUIRE(via_reg_pc(vm)->v_int == test_addr + 2);
        END_SECTION
    END_SECTION

    SECTION("SET")
        vm->acc = foo;
        vm->program[test_addr] = VIA_OP_SET | (VIA_REG_EXPR << 8);
        result = via_run(vm);

        REQUIRE(via_reg_expr(vm) == foo);
    END_SECTION

    SECTION("LOAD")
        via_set_expr(vm, foo);
        vm->program[test_addr] = VIA_OP_LOAD | (VIA_REG_EXPR << 8);
        result = via_run(vm);

        REQUIRE(vm->acc == foo);
    END_SECTION

    SECTION("LOADNIL")
        vm->acc = foo;
        vm->program[test_addr] = VIA_OP_LOADNIL;
        result = via_run(vm);

        REQUIRE(vm->acc == NULL);
    END_SECTION

    SECTION("SETRET")
        vm->acc = foo;
        vm->program[test_addr] = VIA_OP_SETRET;
        result = via_run(vm);

        REQUIRE(vm->ret == foo);
        REQUIRE(result == foo);
    END_SECTION

    SECTION("LOADRET")
        vm->ret = foo;
        vm->program[test_addr] = VIA_OP_LOADRET;
        result = via_run(vm);

        REQUIRE(vm->acc == foo);
    END_SECTION

    SECTION("PAIRP")
        vm->program[test_addr] = VIA_OP_PAIRP;
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
        vm->program[test_addr] = VIA_OP_SYMBOLP;
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
        vm->program[test_addr] = VIA_OP_FORMP;
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
        vm->program[test_addr] = VIA_OP_FRAMEP;
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
        vm->program[test_addr + 3] = VIA_OP_RETURN;
        vm->program[test_addr] = VIA_OP_SKIPZ | (2 << 8);
        vm->acc = foo;

        SECTION("Zero")
            foo->v_int = 0;
            result = via_run(vm);

            REQUIRE(via_reg_pc(vm)->v_int == test_addr + 3);
        END_SECTION
        
        SECTION("Not zero")
            foo->v_int = 1;
            result = via_run(vm);

            REQUIRE(via_reg_pc(vm)->v_int == test_addr + 1);
        END_SECTION
    END_SECTION

    SECTION("PUSH & POP")
        vm->acc = foo;
        vm->ret = bar;
        vm->program[test_addr] = VIA_OP_PUSH;
        vm->program[test_addr + 1] = VIA_OP_LOADRET;
        vm->program[test_addr + 2] = VIA_OP_POP;
        vm->program[test_addr + 3] = VIA_OP_RETURN;
        result = via_run(vm);

        REQUIRE(result == bar);
        REQUIRE(vm->acc == foo);
    END_SECTION

    SECTION("SNAP")
        vm->acc = foo;
        via_set_expr(vm, bar);
        via_set_args(vm, foo);
        via_set_proc(vm, foo);
        via_set_env(vm, foo);
        via_set_exh(vm, foo);
        vm->program[test_addr] = VIA_OP_SNAP | (3 << 8);
        vm->program[test_addr + 1] = VIA_OP_SET| (VIA_REG_EXPR << 8);
        vm->program[test_addr + 2] = VIA_OP_RETURN;
        vm->program[test_addr + 3] = VIA_OP_RETURN;
        vm->program[test_addr + 4] = VIA_OP_RETURN;

        result = via_run(vm);

        REQUIRE(via_reg_pc(vm)->v_int == test_addr + 4);
        REQUIRE(via_reg_expr(vm) == bar);
    END_SECTION

    SECTION("JMP")
        vm->program[test_addr] = VIA_OP_JMP | (4 << 8);
        vm->program[test_addr + 3] = VIA_OP_RETURN;
        vm->program[test_addr + 5] = VIA_OP_JMP | (-3 << 8);
        result = via_run(vm);

        REQUIRE(via_reg_pc(vm)->v_int == test_addr + 3);
    END_SECTION

    SECTION("DROP")
        vm->acc = foo;
        vm->ret = bar;
        vm->program[test_addr] = VIA_OP_PUSH;
        vm->program[test_addr + 1] = VIA_OP_LOADRET;
        vm->program[test_addr + 2] = VIA_OP_PUSH;
        vm->program[test_addr + 3] = VIA_OP_DROP;
        vm->program[test_addr + 4] = VIA_OP_POP;
        vm->program[test_addr + 5] = VIA_OP_RETURN;
        result = via_run(vm);

        REQUIRE(vm->acc == foo);
    END_SECTION

    SECTION("PUSHARG")
        vm->acc = foo;
        vm->program[test_addr] = VIA_OP_PUSHARG;
        result = via_run(vm);

        REQUIRE(via_reg_args(vm)->type == VIA_V_PAIR);
        REQUIRE(via_reg_args(vm)->v_car == foo);
        REQUIRE(via_reg_args(vm)->v_cdr == NULL);
        
        SECTION("POPARG")
            vm->acc = bar;
            via_reg_pc(vm)->v_int = test_addr;
            vm->program[test_addr] = VIA_OP_POPARG;
            result = via_run(vm);

            REQUIRE(vm->acc == foo);
            REQUIRE(via_reg_args(vm) == NULL);
        END_SECTION
    END_SECTION

    via_free_vm(vm);
END_FIXTURE

int main(int argc, char** argv) {
    return RUN_TEST(test_opcodes);
}

