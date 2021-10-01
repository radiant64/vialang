macro(add_embedded BASENAME DATA_FILE VARNAME)
    set(PATH "${CMAKE_CURRENT_BINARY_DIR}/${BASENAME}")
    file(MAKE_DIRECTORY "${$ATH}")
    add_custom_command(
        OUTPUT "${PATH}/${BASENAME}.c" "${PATH}/${BASENAME}.h"
        COMMAND
            ${CMAKE_COMMAND}
                "-DDATA_FILE=${CMAKE_CURRENT_SOURCE_DIR}/${DATA_FILE}"
                "-DVAR_NAME=${VARNAME}"
                "-DHEADER_FILE=${PATH}/${BASENAME}.h"
                "-DIMPL_FILE=${PATH}/${BASENAME}.c"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/embed.cmake
        DEPENDS "${DATA_FILE}"
        VERBATIM
    )
    add_library(${BASENAME} "${PATH}/${BASENAME}.c")
endmacro()

set(SOURCES
    alloc.c
    assembler.c
    builtin.c
    native.c
    parse.c
    vm.c
)
add_embedded(builtin-native builtin.viaas builtin_prg)
add_library(via ${SOURCES})
target_include_directories(via PUBLIC ../include)
target_link_libraries(via PUBLIC m PRIVATE builtin-native)
set_property(TARGET via PROPERTY C_STANDARD 11)


