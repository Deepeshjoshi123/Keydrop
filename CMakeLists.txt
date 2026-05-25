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
        src/core/encoder.cpp
        src/core/packet_builder.cpp
        src/core/packet_reader.cpp
        src/core/endian.cpp
        src/schema/schema_types.cpp
        src/schema/schema_registry.cpp
        src/schema/schema_validator.cpp
        src/schema/field_mapper.cpp
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

add_executable(
    test_packet_reader
    tests/test_packet_reader.cpp
)

target_link_libraries(
    test_packet_reader
    PRIVATE
        keydrop
)

add_test(
    NAME packet_reader
    COMMAND test_packet_reader
)

add_executable(
    test_encoder
    tests/test_encoder.cpp
)

target_link_libraries(
    test_encoder
    PRIVATE
        keydrop
)

add_test(
    NAME encoder
    COMMAND test_encoder
)

add_executable(
    test_schema_types
    tests/test_schema_types.cpp
)

target_link_libraries(
    test_schema_types
    PRIVATE
        keydrop
)

add_test(
    NAME schema_types
    COMMAND test_schema_types
)

add_executable(
    test_schema_registry
    tests/test_schema_registry.cpp
)

target_link_libraries(
    test_schema_registry
    PRIVATE
        keydrop
)

add_test(
    NAME schema_registry
    COMMAND test_schema_registry
)

add_executable(
    test_schema_validator
    tests/test_schema_validator.cpp
)

target_link_libraries(
    test_schema_validator
    PRIVATE
        keydrop
)

add_test(
    NAME schema_validator
    COMMAND test_schema_validator
)

add_executable(
    test_field_mapper
    tests/test_field_mapper.cpp
)

target_link_libraries(
    test_field_mapper
    PRIVATE
        keydrop
)

add_test(
    NAME field_mapper
    COMMAND test_field_mapper
)

# Output
set_target_properties(
    keydrop
    basic_encoding
    test_packet_reader
    test_encoder
    test_schema_types
    test_schema_registry
    test_schema_validator
    test_field_mapper
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
)
