set(SOURCES
    alloc.c
    eval.c
    parse.c
    vm.c
)
add_library(via ${SOURCES})
target_include_directories(via PUBLIC ../include)
set_property(TARGET via PROPERTY C_STANDARD 11)

