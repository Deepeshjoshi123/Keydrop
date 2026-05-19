cmake_minimum_required(VERSION 3.20)

project(
    keydrop
    VERSION 0.1.0
    DESCRIPTION "Adaptive Real-Time Telemetry Transport Engine"
    LANGUAGES CXX
)

# TEMPORARY: GCC 6.3 compatibility
# Later migrate to C++20
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Library
add_library(
    keydrop
    STATIC
)

# Sources
target_sources(
    keydrop
    PRIVATE
        src/core/buffer.cpp
        src/core/packet_builder.cpp
        src/core/packet_reader.cpp
        src/core/endian.cpp
)

# Includes
target_include_directories(
    keydrop
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

# Warnings
if(MSVC)

target_compile_options(
    keydrop
    PRIVATE
    /W4
)

else()

target_compile_options(
    keydrop
    PRIVATE
    -Wall
    -Wextra
)

endif()

# Example
add_executable(
    basic_encoding
    examples/basic_encoding.cpp
)

target_link_libraries(
    basic_encoding
    PRIVATE
        keydrop
)

# Testing
enable_testing()

# Output
set_target_properties(
    keydrop
    basic_encoding
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
)