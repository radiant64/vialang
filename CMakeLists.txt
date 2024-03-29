cmake_minimum_required(VERSION 3.12)
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(SEND_ERROR
        "In-source build prevented. Please run CMake from a separate folder (\
making a new subfolder called \"build/\" is recommended)."
    )
endif()

project(via
    VERSION 0.1.1
    DESCRIPTION "Embeddable Via interpreter"
)
include(CTest)
enable_testing()

set(CMAKE_C_STANDARD_REQUIRED 1)

configure_file(via.pc.in via.pc @ONLY)
include(GNUInstallDirs)
install(
    FILES ${CMAKE_BINARY_DIR}/via.pc
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig
)

add_subdirectory(src)
add_subdirectory(tests)

if(NOT DISABLE_CPP)
    add_subdirectory(via-cpp)
endif()

add_executable(standalone src/standalone.c)
target_link_libraries(standalone PRIVATE via)
target_compile_definitions(standalone PRIVATE VIA_VERSION="${PROJECT_VERSION}")
set_target_properties(standalone PROPERTIES C_STANDARD 11 OUTPUT_NAME via)

install(TARGETS standalone)

