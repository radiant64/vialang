#include <testdrive.h>

#include <via/asm_macros.h>
#include <via/vm.h>

static void test_add(struct via_vm* vm) {
    struct via_value* a = via_get(vm, "a");
    struct via_value* b = via_get(vm, "b");

    vm->ret = via_make_int(vm, a->v_int - b->v_int);
}

static void test_form(struct via_vm* vm) {
    vm->ret = via_context(vm)->v_car;
}

static void test_car(struct via_vm* vm) {
    vm->ret = via_pop_arg(vm)->v_car;
}

static void test_cdr(struct via_vm* vm) {
    vm->ret = via_pop_arg(vm)->v_cdr;
}

static void test_context(struct via_vm* vm) {
    vm->ret = via_context(vm);
}

static void test_throw(struct via_vm* vm) {
    via_throw(vm, via_make_int(vm, 123));
}

FIXTURE(test_eval, "Eval")
    struct via_vm* vm = via_create_vm();
    REQUIRE(vm);

    struct via_value* result;

    SECTION("Literal value")
        vm->regs->v_arr[VIA_REG_EXPR] = via_make_value(vm);
        vm->regs->v_arr[VIA_REG_EXPR]->type = VIA_V_INT;

        result = via_run(vm);

        REQUIRE(result == vm->regs->v_arr[VIA_REG_EXPR]);
    END_SECTION

    SECTION("Symbol lookup")
        struct via_value* value = via_make_value(vm);

        via_env_set(vm, via_sym(vm, "test-symbol"), value); 
        vm->regs->v_arr[VIA_REG_EXPR] = via_sym(vm, "test-symbol");

        result = via_run(vm);

        REQUIRE(result == value);
    END_SECTION

    SECTION("Procedure application")
        struct via_value* formals = via_make_pair(
            vm,
            via_sym(vm, "value"),
            NULL
        );

        struct via_value* proc = via_list(
            vm,
            via_sym(vm, "value"),
            via_formals(vm, "value", NULL),
            vm->regs->v_arr[VIA_REG_ENV],
            NULL
        );
        proc->type = VIA_V_PROC;

        // Compound expression
        vm->regs->v_arr[VIA_REG_EXPR] = via_make_pair(
            vm,
            proc,
            via_make_pair(vm, via_make_int(vm, 123), NULL)
        );

        result = via_run(vm);

        REQUIRE(result->type == VIA_V_INT);
        REQUIRE(result->v_int == 123);
    END_SECTION
    
    SECTION("Procedure application (builtin)")
        struct via_value* formals = via_formals(vm, "a", "b", NULL);
        via_register_proc(vm, "test-add", formals, test_add);

        vm->regs->v_arr[VIA_REG_EXPR] = via_list(
            vm,
            via_sym(vm, "test-add"),
            via_make_int(vm, 34),
            via_make_int(vm, 12),
            NULL
        );

        result = via_run(vm);

        REQUIRE(result->type == VIA_V_INT);
        REQUIRE(result->v_int == 22);
    END_SECTION

    SECTION("Special forms")
        vm->regs->v_arr[VIA_REG_EXPR] = via_make_pair(
            vm,
            via_sym(vm, "test-form"),
            via_make_pair(vm, via_sym(vm, "test-symbol"), NULL)
        );

        SECTION("Native") 
            via_register_proc(vm, "car", NULL, test_car);
            via_register_proc(vm, "cdr", NULL, test_cdr);
            via_register_proc(vm, "context", NULL, test_context);
            struct via_value* form = via_make_pair(
                vm,
                via_make_pair(
                    vm,
                    via_sym(vm, "car"),
                    via_make_pair(
                        vm,
                        via_make_pair(
                            vm,
                            via_sym(vm, "context"),
                            NULL
                        ),
                        NULL
                    )
                ),
                NULL
            );
            form->type = VIA_V_FORM;

            via_env_set(vm, via_sym(vm, "test-form"), form);

            result = via_run(vm);

            REQUIRE(result == via_sym(vm, "test-symbol"));
        END_SECTION

        SECTION("Built in")
            via_register_form(vm, "test-form", test_form);
            result = via_run(vm);

            REQUIRE(result == via_sym(vm, "test-symbol"));
        END_SECTION
    END_SECTION

    SECTION("Exceptions")
        via_register_proc(vm, "throw-proc", NULL, test_throw);
        vm->regs->v_arr[VIA_REG_EXPR] = via_list(
            vm,
            via_sym(vm, "throw-proc"),
            NULL
        );
        
        result = via_run(vm);

        REQUIRE(result->type == VIA_V_INT);
        REQUIRE(result->v_int == 123);
    END_SECTION

    via_free_vm(vm);
END_FIXTURE

int main(int argc, char** argv) {
    return RUN_TEST(test_eval);
}

