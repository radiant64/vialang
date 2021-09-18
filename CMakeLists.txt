cmake_minimum_required(VERSION 3.5)
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(SEND_ERROR
        "In-source build prevented. Please run CMake from a separate folder (\
making a new subfolder called \"build/\" is recommended)."
    )
endif()

project(via)
include(CTest)
enable_testing()

set(CMAKE_C_STANDARD_REQUIRED 1)

add_subdirectory(src)
add_subdirectory(tests)

