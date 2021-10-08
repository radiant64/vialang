#include <testdrive.h>

#include <via/assembler.h>
#include <via/vm.h>

FIXTURE(test_assembler, "Assembler")
    struct via_vm* vm = via_create_vm();
    REQUIRE(vm);

    SECTION("Instructions (no operands)")
        const char* source =
            "  nop ; This is a comment\n"
            "  car \n"
            "  cdr\n"
            "callacc\n"
            "loadnil\n"
            "setret\n"
            "loadret\n"
            "pairp\n"
            "symbolp\n"
            "formp\n"
            "framep\n"
            "builtinp\n"
            "return\n"
            "push\n"
            "pop\n"
            "drop\n"
            "pusharg\n"
            "poparg";
        struct via_assembly_result result = via_assemble(vm, source);

        const via_opcode expected[] = {
            VIA_OP_NOP,
            VIA_OP_CAR,
            VIA_OP_CDR,
            VIA_OP_CALLACC,
            VIA_OP_SETRET,
            VIA_OP_LOADRET,
            VIA_OP_PAIRP,
            VIA_OP_SYMBOLP,
            VIA_OP_FORMP,
            VIA_OP_FRAMEP,
            VIA_OP_BUILTINP,
            VIA_OP_RETURN,
            VIA_OP_PUSH,
            VIA_OP_POP,
            VIA_OP_DROP,
            VIA_OP_PUSHARG,
            VIA_OP_POPARG
        };
        REQUIRE(result.status == VIA_ASM_SUCCESS);
        for (int i = 0; i < sizeof(expected) / sizeof(via_opcode); ++i) {
            REQUIRE(vm->program[result.addr + i] == expected[i]);
        }
    END_SECTION
    
    SECTION("Labels & operands")
        const char* source =
            "foo:\n"
            "    call foo\n"
            "    load !args\n"
            "    skipz @bar\n"
            "        snap @bar\n"
            "            jmp foo\n"
            "@bar:\n"
            "    callb @bar\n"
            "    set !exh";
        struct via_assembly_result result = via_assemble(vm, source);
        const via_opcode expected[] = {
            VIA_OP_CALL | (result.addr << 8),
            VIA_OP_LOAD | (VIA_REG_ARGS << 8),
            VIA_OP_SKIPZ | (2 << 8),
            VIA_OP_SNAP | (1 << 8),
            VIA_OP_JMP | (-5 << 8),
            VIA_OP_CALLB | ((result.addr + 5) << 8),
            VIA_OP_SET | (VIA_REG_EXH << 8) 
        };
        REQUIRE(result.status == VIA_ASM_SUCCESS);
        for (int i = 0; i < sizeof(expected) / sizeof(via_opcode); ++i) {
            REQUIRE(vm->program[result.addr + i] == expected[i]);
        }
        REQUIRE(vm->labels_count == 1);
        REQUIRE(strcmp(vm->labels[0], "foo") == 0);
        REQUIRE(vm->label_addrs[0] == result.addr);
    END_SECTION

    via_free_vm(vm);
END_FIXTURE

int main(int argc, char** argv) {
    return RUN_TEST(test_assembler);
}

