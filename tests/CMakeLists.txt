macro(create_test_target TEST)
    add_executable(${TEST} ${ARGN})
    set_property(TARGET ${TEST} PROPERTY C_STANDARD 99)
    target_include_directories(${TEST} PRIVATE include)
    target_link_libraries(${TEST} via)
    add_test(NAME ${TEST} COMMAND ${TEST})
endmacro()

create_test_target(test_opcodes test_opcodes.c)
create_test_target(test_assembler test_assembler.c)
create_test_target(test_eval test_eval.c)
create_test_target(test_parse test_parse.c)
create_test_target(test_programs test_programs.c)

