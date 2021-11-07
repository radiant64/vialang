#include <testdrive.h>

#include <via/exceptions.h>
#include <via/type-utils.h>
#include <via/vm.h>

static void test_add(struct via_vm* vm) {
    const struct via_value* a = via_get(vm, "a");
    const struct via_value* b = via_get(vm, "b");

    vm->ret = via_make_int(vm, a->v_int - b->v_int);
}

static void test_form(struct via_vm* vm) {
    vm->ret = via_reg_ctxt(vm)->v_cdr->v_car;
}

static void test_car(struct via_vm* vm) {
    vm->ret = via_pop_arg(vm)->v_car;
}

static void test_cdr(struct via_vm* vm) {
    vm->ret = via_pop_arg(vm)->v_cdr;
}

static void test_context(struct via_vm* vm) {
    vm->ret = via_reg_ctxt(vm);
}

static void test_throw(struct via_vm* vm) {
    via_throw(vm, via_except_runtime_error(vm, "test"));
}

FIXTURE(test_eval, "Eval")
    struct via_vm* vm = via_create_vm();
    REQUIRE(vm);

    const struct via_value* result;

    SECTION("Literal value")
        via_set_expr(vm, via_make_int(vm, 0));

        result = via_run_eval(vm);

        REQUIRE(result == via_reg_expr(vm));
    END_SECTION

    SECTION("Symbol lookup")
        struct via_value* value = via_make_value(vm);

        via_env_set(vm, via_sym(vm, "test-symbol"), value); 
        via_set_expr(vm, via_sym(vm, "test-symbol"));

        result = via_run_eval(vm);

        REQUIRE(result == value);
    END_SECTION

    SECTION("Procedure application")
        const struct via_value* proc = via_make_proc(
            vm,
            via_sym(vm, "value"),
            via_formals(vm, "value", NULL),
            via_reg_env(vm)
        );

        // Compound expression
        via_set_expr(
            vm,
            via_list(
                vm,
                proc,
                via_make_int(vm, 123),
                NULL
            )
        );

        result = via_run_eval(vm);

        REQUIRE(result->type == VIA_V_INT);
        REQUIRE(result->v_int == 123);
    END_SECTION
    
    SECTION("Procedure application (builtin)")
        const struct via_value* formals = via_formals(vm, "a", "b", NULL);
        via_register_proc(
            vm,
            "test-add",
            "test-add-proc",
            formals,
            (via_bindable) test_add
        );

        via_set_expr(
            vm,
            via_list(
                vm,
                via_sym(vm, "test-add"),
                via_make_int(vm, 34),
                via_make_int(vm, 12),
                NULL
            )
        );

        result = via_run_eval(vm);

        REQUIRE(result->type == VIA_V_INT);
        REQUIRE(result->v_int == 22);
    END_SECTION

    SECTION("Special forms")
        via_set_expr(
            vm,
            via_list(
                vm,
                via_sym(vm, "test-form"),
                via_sym(vm, "test-symbol"),
                NULL
            )
        );

        SECTION("Built in")
            via_register_form(
                vm,
                "test-form",
                "test-form-proc",
                NULL,
                (via_bindable) test_form
            );
            result = via_run_eval(vm);

            REQUIRE(result == via_sym(vm, "test-symbol"));
        END_SECTION
    END_SECTION

    SECTION("Exceptions")
        via_register_proc(
            vm,
            "throw-proc",
            "throw-proc",
            NULL, 
            (via_bindable) test_throw
        );
        via_set_expr(vm,
            via_list(
                vm,
                via_sym(vm, "throw-proc"),
                NULL
            )
        );
        
        result = via_run_eval(vm);

        REQUIRE(via_is_exception(vm, result));
    END_SECTION

    SECTION("Garbage collection")
        via_int start = vm->heap_top + 1;
        const struct via_value* foo = via_make_string(vm, "foo");
        const struct via_value* bar = via_make_string(vm, "bar");
        const struct via_value* baz = via_make_string(vm, "baz");
        const struct via_value* quux = via_make_string(vm, "quux");
        const struct via_value* disposable = via_make_string(vm, "test");
        
        REQUIRE(vm->heap[start] == foo);
        REQUIRE(vm->heap[start + 1] == bar);
        REQUIRE(vm->heap[start + 2] == baz);
        REQUIRE(vm->heap[start + 3] == quux);
        REQUIRE(vm->heap[start + 4] == disposable);

        via_set_args(vm, via_list(vm, foo, bar, baz, NULL));
        const struct via_value* frame = via_make_frame(vm);
        vm->regs = (struct via_value*) frame;
        via_set_expr(vm, quux);

        via_garbage_collect(vm);

        REQUIRE(vm->heap[start + 0] == foo);
        REQUIRE(vm->heap[start + 1] == bar);
        REQUIRE(vm->heap[start + 2] == baz);
        REQUIRE(vm->heap[start + 3] == quux);
        REQUIRE(!vm->heap[start + 4]);

        vm->regs = (struct via_value*) via_reg_parn(vm);

        via_garbage_collect(vm);
        
        REQUIRE(vm->heap[start + 0] == foo);
        REQUIRE(vm->heap[start + 1] == bar);
        REQUIRE(vm->heap[start + 2] == baz);
        REQUIRE(!vm->heap[start + 3]);
    END_SECTION
    via_free_vm(vm);
END_FIXTURE

int main(int argc, char** argv) {
    return RUN_TEST(test_eval);
}

