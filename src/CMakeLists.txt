macro(link_embedded TARGET BASENAME DATA_FILE APPEND_ZERO VARNAME)
    set(PATH "${CMAKE_CURRENT_BINARY_DIR}/${BASENAME}")
    file(MAKE_DIRECTORY "${$ATH}")
    add_custom_command(
        OUTPUT "${PATH}/${BASENAME}.c" "${PATH}/${BASENAME}.h"
        COMMAND
            ${CMAKE_COMMAND}
                "-DDATA_FILE=${CMAKE_CURRENT_SOURCE_DIR}/${DATA_FILE}"
                "-DVAR_NAME=${VARNAME}"
                "-DAPPEND_ZERO=${APPEND_ZERO}"
                "-DHEADER_FILE=${PATH}/${BASENAME}.h"
                "-DIMPL_FILE=${PATH}/${BASENAME}.c"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/embed.cmake
        DEPENDS "${DATA_FILE}"
        VERBATIM
    )
    target_sources(${TARGET} PRIVATE "${PATH}/${BASENAME}.c")
    target_include_directories(${TARGET} PRIVATE "${PATH}")
endmacro()

set(SOURCES
    alloc.c
    assembler.c
    builtin.c
    exceptions.c
    parse.c
    vm.c
)
if(SHARED_LIBRARY)
    add_library(${PROJECT_NAME} SHARED ${SOURCES} )
else()
    add_library(${PROJECT_NAME} STATIC ${SOURCES})
endif()
link_embedded(${PROJECT_NAME} builtin-native builtin.viaas 1 builtin_prg)
link_embedded(${PROJECT_NAME} native-via native.via 1 native_via)
target_include_directories(${PROJECT_NAME} PUBLIC ../include)
target_link_libraries(${PROJECT_NAME} PUBLIC m)
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    C_STANDARD 11
)

install(
    TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
    DIRECTORY ../include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

