project(via-cpp
    VERSION 1.0.0
    DESCRIPTION "C++ Wrapper for the Via embeddable interpreter"
)
add_library(via-cpp INTERFACE)
target_include_directories(via-cpp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(via-cpp INTERFACE cxx_std_17)

configure_file(via-cpp.pc.in via-cpp.pc @ONLY)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/via-cpp.pc
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig
)
install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

