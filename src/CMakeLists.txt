set(SOURCES
    alloc.c
    assembler.c
    builtin.c
    native.c
    parse.c
    vm.c
)
add_library(via ${SOURCES})
target_include_directories(via PUBLIC ../include)
target_link_libraries(via PUBLIC m)
set_property(TARGET via PROPERTY C_STANDARD 11)

