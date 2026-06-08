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
        src/reliability/corruption_detector.cpp
        src/reliability/packet_synchronizer.cpp
        src/schema/schema_types.cpp
        src/schema/schema_registry.cpp
        src/schema/schema_validator.cpp
        src/schema/field_mapper.cpp
        src/schema/json_types.cpp
        src/schema/runtime_optimizer.cpp
        src/schema/adaptive_dictionary.cpp
        src/schema/stream_optimizer.cpp
        src/schema/schema_runtime.cpp
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

add_executable(
    test_schema_runtime
    tests/test_schema_runtime.cpp
)

target_link_libraries(
    test_schema_runtime
    PRIVATE
        keydrop
)

add_test(
    NAME schema_runtime
    COMMAND test_schema_runtime
)

add_executable(
    test_runtime_optimizer
    tests/test_runtime_optimizer.cpp
)

target_link_libraries(
    test_runtime_optimizer
    PRIVATE
        keydrop
)

add_test(
    NAME runtime_optimizer
    COMMAND test_runtime_optimizer
)

add_executable(
    test_adaptive_dictionary
    tests/test_adaptive_dictionary.cpp
)

target_link_libraries(
    test_adaptive_dictionary
    PRIVATE
        keydrop
)

add_test(
    NAME adaptive_dictionary
    COMMAND test_adaptive_dictionary
)

add_executable(
    test_stream_optimizer
    tests/test_stream_optimizer.cpp
)

target_link_libraries(
    test_stream_optimizer
    PRIVATE
        keydrop
)

add_test(
    NAME stream_optimizer
    COMMAND test_stream_optimizer
)

add_executable(
    test_corruption_detector
    tests/test_corruption_detector.cpp
)

target_link_libraries(
    test_corruption_detector
    PRIVATE
        keydrop
)

add_test(
    NAME corruption_detector
    COMMAND test_corruption_detector
)

add_executable(
    test_packet_synchronizer
    tests/test_packet_synchronizer.cpp
)

target_link_libraries(
    test_packet_synchronizer
    PRIVATE
        keydrop
)

add_test(
    NAME packet_synchronizer
    COMMAND test_packet_synchronizer
)

add_executable(
    stream_optimizer_benchmark
    benchmarks/stream_optimizer_benchmark.cpp
)

target_link_libraries(
    stream_optimizer_benchmark
    PRIVATE
        keydrop
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
    test_schema_runtime
    test_runtime_optimizer
    test_adaptive_dictionary
    test_stream_optimizer
    test_corruption_detector
    test_packet_synchronizer
    stream_optimizer_benchmark
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
)
