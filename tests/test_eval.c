#include <testdrive.h>

#include <via/asm_macros.h>
#include <via/vm.h>

static void test_add(struct via_vm* vm) {
    struct via_value* a = via_get(vm, "a");
    struct via_value* b = via_get(vm, "b");

    vm->ret = via_make_int(vm, a->v_int + b->v_int);
}

static void test_form(struct via_vm* vm) {
    vm->ret = via_context(vm)->v_cdr->v_car;
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
        struct via_value* value = via_make_value(vm);

        via_env_set(vm, via_symbol(vm, "test-symbol"), value); 
        vm->regs[VIA_REG_EXPR] = via_symbol(vm, "test-symbol");

        result = via_run(vm);

        REQUIRE(result == value);
    END_SECTION

    SECTION("Procedure application")
        struct via_value* formals = via_make_pair(
            vm,
            via_symbol(vm, "value"),
            NULL
        );

        struct via_value* proc = via_make_pair(
            vm,
            via_symbol(vm, "value"),
            via_make_pair(
                vm,
                formals,
                via_make_pair(
                    vm,
                    vm->regs[VIA_REG_ENV],
                    NULL
                )
            )
        );
        proc->type = VIA_V_PROC;

        // Compound expression
        vm->regs[VIA_REG_EXPR] = via_make_pair(
            vm,
            proc,
            via_make_pair(vm, via_make_int(vm, 123), NULL)
        );

        result = via_run(vm);

        REQUIRE(result->type == VIA_V_INT);
        REQUIRE(result->v_int == 123);
    END_SECTION
    
    SECTION("Procedure application (builtin)")
        struct via_value* formals = via_make_pair(
            vm,
            via_symbol(vm, "a"),
            via_make_pair(vm, via_symbol(vm, "b"), NULL)
        );
        via_register_proc(vm, "test-add", formals, test_add);

        vm->regs[VIA_REG_EXPR] = via_make_pair(
            vm,
            via_symbol(vm, "test-add"),
            via_make_pair(
                vm,
                via_make_int(vm, 12),
                via_make_pair(vm, via_make_int(vm, 34), NULL)
            )
        );

        result = via_run(vm);

        REQUIRE(result->type == VIA_V_INT);
        REQUIRE(result->v_int == 46);
    END_SECTION

    SECTION("Special forms")
        vm->regs[VIA_REG_EXPR] = via_make_pair(
            vm,
            via_symbol(vm, "test-form"),
            via_make_pair(vm, via_symbol(vm, "test-symbol"), NULL)
        );
            
        SECTION("Native") 
            via_register_proc(vm, "car", NULL, test_car);
            via_register_proc(vm, "cdr", NULL, test_cdr);
            via_register_proc(vm, "context", NULL, test_context);
            struct via_value* form = via_make_pair(
                vm,
                via_make_pair(
                    vm,
                    via_symbol(vm, "car"),
                    via_make_pair(
                        vm,
                        via_make_pair(
                            vm,
                            via_symbol(vm, "cdr"),
                            via_make_pair(
                                vm,
                                via_make_pair(
                                    vm,
                                    via_symbol(vm, "context"),
                                    NULL
                                ),
                                NULL
                            )
                        ),
                        NULL
                    )
                ),
                NULL
            );
            form->type = VIA_V_FORM;

            via_env_set(vm, via_symbol(vm, "test-form"), form);

            result = via_run(vm);

            REQUIRE(result == via_symbol(vm, "test-symbol"));
        END_SECTION

        SECTION("Built in")
            via_register_form(vm, "test-form", test_form);
            result = via_run(vm);

            REQUIRE(result == via_symbol(vm, "test-symbol"));
        END_SECTION

    END_SECTION

    via_free_vm(vm);
END_FIXTURE

int main(int argc, char** argv) {
    return RUN_TEST(test_eval);
}

