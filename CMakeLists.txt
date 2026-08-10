cmake_minimum_required(VERSION 3.20)

project(
    keydrop
    VERSION 0.1.0
    DESCRIPTION "Adaptive Real-Time Telemetry Transport Engine"
    LANGUAGES CXX
)

find_package(Threads REQUIRED)

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
        src/benchmark/benchmark_core.cpp
        src/benchmark/format_benchmark.cpp
        src/benchmark/benchmark_report.cpp
        src/benchmark/heap_tracker.cpp
        src/platform/socket.cpp
        src/core/buffer.cpp
        src/core/buffer_pool.cpp
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
        src/schema/schema_config.cpp
        src/schema/fast_codec.cpp
        src/schema/profiles.cpp
        src/schema/adaptive_profiler.cpp
        src/schema/packet_layout.cpp
        src/schema/runtime_optimizer.cpp
        src/schema/adaptive_dictionary.cpp
        src/schema/payload_pool.cpp
        src/schema/stream_optimizer.cpp
        src/schema/schema_runtime.cpp
        src/transport/transport.cpp
        src/transport/tcp_adapter.cpp
        src/transport/websocket_adapter.cpp
        src/transport/udp_adapter.cpp
        src/transport/transport_config.cpp
        src/transport/transport_scheduler.cpp
)

# Includes
target_include_directories(
    keydrop
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

# Static-library consumers must receive the platform system libraries too.
target_link_libraries(keydrop PUBLIC Threads::Threads)

if(WIN32)
    target_link_libraries(keydrop PUBLIC ws2_32)
endif()

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

# Legacy example (kept for CI test compatibility)
add_executable(
    basic_encoding
    examples/basic_encoding.cpp
)
target_link_libraries(
    basic_encoding
    PRIVATE
        keydrop
)

add_executable(keydrop_cli tools/keydrop_cli.cpp)

# Phase 7: official external baselines. Separate targets and study
# classification — never conflated with the in-repository baselines.
find_package(Protobuf QUIET)
find_package(nlohmann_json QUIET)
find_path(MSGPACK_INCLUDE_DIR msgpack.hpp)
find_library(MSGPACK_LIBRARY NAMES msgpackc)

set(OFFICIAL_BENCH_SOURCES
    benchmarks/official_baselines_benchmark.cpp
)

if(Protobuf_FOUND)
    protobuf_generate_cpp(
        OFFICIAL_PROTO_SRCS
        OFFICIAL_PROTO_HDRS
        ${PROJECT_SOURCE_DIR}/benchmarks/proto/telemetry.proto
    )
    list(APPEND OFFICIAL_BENCH_SOURCES ${OFFICIAL_PROTO_SRCS})
endif()

add_executable(
    official_baselines_benchmark
    ${OFFICIAL_BENCH_SOURCES}
)

target_link_libraries(
    official_baselines_benchmark
    PRIVATE
        keydrop
)

if(Protobuf_FOUND)
    target_compile_definitions(official_baselines_benchmark PRIVATE KEYDROP_HAS_PROTOBUF)
    target_link_libraries(official_baselines_benchmark PRIVATE protobuf::libprotobuf)
    target_include_directories(official_baselines_benchmark PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(nlohmann_json_FOUND)
    target_compile_definitions(official_baselines_benchmark PRIVATE KEYDROP_HAS_JSON)
    target_link_libraries(official_baselines_benchmark PRIVATE nlohmann_json::nlohmann_json)
endif()

if(MSGPACK_INCLUDE_DIR AND MSGPACK_LIBRARY)
    target_compile_definitions(official_baselines_benchmark PRIVATE KEYDROP_HAS_MSGPACK)
    target_include_directories(official_baselines_benchmark PRIVATE ${MSGPACK_INCLUDE_DIR})
    target_link_libraries(official_baselines_benchmark PRIVATE ${MSGPACK_LIBRARY})
endif()

add_test(NAME official_baselines COMMAND official_baselines_benchmark 2000)
target_link_libraries(keydrop_cli PRIVATE keydrop)

# Integration examples
add_subdirectory(integration)

# Testing
enable_testing()

add_executable(
    test_buffer
    tests/test_buffer.cpp
)

target_link_libraries(
    test_buffer
    PRIVATE
        keydrop
)

add_test(
    NAME buffer
    COMMAND test_buffer
)

add_executable(
    test_buffer_pool
    tests/test_buffer_pool.cpp
)

target_link_libraries(
    test_buffer_pool
    PRIVATE
        keydrop
)

add_test(
    NAME buffer_pool
    COMMAND test_buffer_pool
)

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

add_executable(test_schema_config tests/test_schema_config.cpp)
target_link_libraries(test_schema_config PRIVATE keydrop)
add_test(NAME schema_config COMMAND test_schema_config)

add_executable(test_fast_codec tests/test_fast_codec.cpp)
target_link_libraries(test_fast_codec PRIVATE keydrop)
add_test(NAME fast_codec COMMAND test_fast_codec)

add_executable(test_delta_stream tests/test_delta_stream.cpp)
target_link_libraries(test_delta_stream PRIVATE keydrop)
add_test(NAME delta_stream COMMAND test_delta_stream)

add_executable(test_adaptive_profiler tests/test_adaptive_profiler.cpp)
target_link_libraries(test_adaptive_profiler PRIVATE keydrop)
add_test(NAME adaptive_profiler COMMAND test_adaptive_profiler)

add_executable(test_fuzz_reliability tests/test_fuzz_reliability.cpp)
target_link_libraries(test_fuzz_reliability PRIVATE keydrop)
add_test(NAME fuzz_reliability COMMAND test_fuzz_reliability)

add_executable(test_compatibility tests/test_compatibility.cpp)
target_link_libraries(test_compatibility PRIVATE keydrop)
add_test(NAME compatibility COMMAND test_compatibility)

add_executable(test_tcp_reliability tests/test_tcp_reliability.cpp)
target_link_libraries(test_tcp_reliability PRIVATE keydrop)
add_test(NAME tcp_reliability COMMAND test_tcp_reliability)
set_tests_properties(tcp_reliability PROPERTIES SKIP_RETURN_CODE 77)

add_executable(test_websocket_handshake tests/test_websocket_handshake.cpp)
target_link_libraries(test_websocket_handshake PRIVATE keydrop)
add_test(NAME websocket_handshake COMMAND test_websocket_handshake)
set_tests_properties(websocket_handshake PROPERTIES SKIP_RETURN_CODE 77)

add_executable(test_udp_adapter tests/test_udp_adapter.cpp)
target_link_libraries(test_udp_adapter PRIVATE keydrop)
add_test(NAME udp_adapter COMMAND test_udp_adapter)
set_tests_properties(udp_adapter PROPERTIES SKIP_RETURN_CODE 77)

add_executable(
    test_packet_layout
    tests/test_packet_layout.cpp
)

target_link_libraries(
    test_packet_layout
    PRIVATE
        keydrop
)

add_test(
    NAME packet_layout
    COMMAND test_packet_layout
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
    test_payload_pool
    tests/test_payload_pool.cpp
)

target_link_libraries(
    test_payload_pool
    PRIVATE
        keydrop
)

add_test(
    NAME payload_pool
    COMMAND test_payload_pool
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
    test_transport
    tests/test_transport.cpp
)

target_link_libraries(
    test_transport
    PRIVATE
        keydrop
)

add_test(
    NAME transport
    COMMAND test_transport
)

add_executable(
    test_tcp_adapter
    tests/test_tcp_adapter.cpp
)

target_link_libraries(
    test_tcp_adapter
    PRIVATE
        keydrop
)

add_test(
    NAME tcp_adapter
    COMMAND test_tcp_adapter
)

set_tests_properties(
    tcp_adapter
    PROPERTIES
    SKIP_RETURN_CODE 77
)

add_executable(
    test_websocket_adapter
    tests/test_websocket_adapter.cpp
)

target_link_libraries(
    test_websocket_adapter
    PRIVATE
        keydrop
)

add_test(
    NAME websocket_adapter
    COMMAND test_websocket_adapter
)

set_tests_properties(
    websocket_adapter
    PROPERTIES
    SKIP_RETURN_CODE 77
)

add_executable(
    test_transport_scheduler
    tests/test_transport_scheduler.cpp
)

target_link_libraries(
    test_transport_scheduler
    PRIVATE
        keydrop
)

add_test(
    NAME transport_scheduler
    COMMAND test_transport_scheduler
)

add_executable(
    test_benchmark_core
    tests/test_benchmark_core.cpp
)

target_link_libraries(
    test_benchmark_core
    PRIVATE
        keydrop
)

add_test(
    NAME benchmark_core
    COMMAND test_benchmark_core
)

add_executable(
    test_format_benchmark
    tests/test_format_benchmark.cpp
)

target_link_libraries(
    test_format_benchmark
    PRIVATE
        keydrop
)

add_test(
    NAME format_benchmark
    COMMAND test_format_benchmark
)

add_executable(
    test_benchmark_report
    tests/test_benchmark_report.cpp
)

target_link_libraries(
    test_benchmark_report
    PRIVATE
        keydrop
)

add_test(
    NAME benchmark_report
    COMMAND test_benchmark_report
)

add_executable(
    benchmark_runner
    benchmarks/benchmark_runner.cpp
)

target_link_libraries(
    benchmark_runner
    PRIVATE
        keydrop
)

add_test(
    NAME benchmark_runner
    COMMAND benchmark_runner 4 ${PROJECT_BINARY_DIR}/benchmark_report.txt
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

add_executable(
    fastpath_benchmark
    benchmarks/fastpath_benchmark.cpp
)

target_link_libraries(
    fastpath_benchmark
    PRIVATE
        keydrop
)

add_test(
    NAME fastpath_benchmark
    COMMAND fastpath_benchmark 20000
)

add_executable(
    stateful_bandwidth_benchmark
    benchmarks/stateful_bandwidth_benchmark.cpp
)

target_link_libraries(
    stateful_bandwidth_benchmark
    PRIVATE
        keydrop
)

add_executable(
    adaptive_profiles_benchmark
    benchmarks/adaptive_profiles_benchmark.cpp
)

target_link_libraries(
    adaptive_profiles_benchmark
    PRIVATE
        keydrop
)

add_executable(
    transport_bytes_benchmark
    benchmarks/transport_bytes_benchmark.cpp
)

target_link_libraries(
    transport_bytes_benchmark
    PRIVATE
        keydrop
)

target_include_directories(
    transport_bytes_benchmark
    PRIVATE
        ${PROJECT_SOURCE_DIR}/tests
)

# Tests must retain assertion checks in Release benchmark builds.  Without this,
# NDEBUG can compile out both checks and accidental side effects inside asserts.
set(KEYDROP_TEST_TARGETS
    test_buffer
    test_buffer_pool
    test_packet_reader
    test_encoder
    test_schema_types
    test_schema_config
    test_fast_codec
    test_delta_stream
    test_adaptive_profiler
    test_fuzz_reliability
    test_compatibility
    test_tcp_reliability
    test_websocket_handshake
    test_udp_adapter
    test_packet_layout
    test_schema_registry
    test_schema_validator
    test_field_mapper
    test_payload_pool
    test_schema_runtime
    test_runtime_optimizer
    test_adaptive_dictionary
    test_stream_optimizer
    test_corruption_detector
    test_packet_synchronizer
    test_transport
    test_tcp_adapter
    test_websocket_adapter
    test_transport_scheduler
    test_benchmark_core
    test_format_benchmark
    test_benchmark_report
)

foreach(test_target IN LISTS KEYDROP_TEST_TARGETS)
    if(MSVC)
        target_compile_options(${test_target} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${test_target} PRIVATE -UNDEBUG)
    endif()
endforeach()

# Output
set_target_properties(
    keydrop
    basic_encoding
    test_buffer
    test_buffer_pool
    test_packet_reader
    test_encoder
    test_schema_types
    test_packet_layout
    test_schema_registry
    test_schema_validator
    test_field_mapper
    test_payload_pool
    test_schema_runtime
    test_runtime_optimizer
    test_adaptive_dictionary
    test_stream_optimizer
    test_corruption_detector
    test_packet_synchronizer
    test_transport
    test_tcp_adapter
    test_websocket_adapter
    test_transport_scheduler
    test_benchmark_core
    test_format_benchmark
    test_benchmark_report
    benchmark_runner
    stream_optimizer_benchmark
    test_schema_config
    test_fast_codec
    test_delta_stream
    test_adaptive_profiler
    test_fuzz_reliability
    test_compatibility
    test_tcp_reliability
    test_websocket_handshake
    test_udp_adapter
    fastpath_benchmark
    stateful_bandwidth_benchmark
    adaptive_profiles_benchmark
    transport_bytes_benchmark
    official_baselines_benchmark
    keydrop_cli
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
)
