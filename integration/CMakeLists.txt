# Build all integration examples.
#
# Each example is an independent executable that links against the
# Keydrop static library.  Build from the repository root:
#
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --target 01-send-receive
#   ./build/integration/01-send-receive
#
# Or build everything at once:
#   cmake --build build --target all_integration

# ---- 01: send-receive ------------------------------------------
add_executable(01-send-receive 01-send-receive.cpp)
target_link_libraries(01-send-receive PRIVATE keydrop)
set_target_properties(01-send-receive PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/integration"
)

# ---- 02: adaptive dictionary -----------------------------------
add_executable(02-with-dictionary 02-with-dictionary.cpp)
target_link_libraries(02-with-dictionary PRIVATE keydrop)
set_target_properties(02-with-dictionary PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/integration"
)

# ---- 03: stream batching ---------------------------------------
add_executable(03-stream-batching 03-stream-batching.cpp)
target_link_libraries(03-stream-batching PRIVATE keydrop)
set_target_properties(03-stream-batching PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/integration"
)

# ---- 04: TCP server --------------------------------------------
add_executable(04-tcp-server 04-tcp-server.cpp)
target_link_libraries(04-tcp-server PRIVATE keydrop)
set_target_properties(04-tcp-server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/integration"
)

# ---- 04: TCP client --------------------------------------------
add_executable(04-tcp-client 04-tcp-client.cpp)
target_link_libraries(04-tcp-client PRIVATE keydrop)
set_target_properties(04-tcp-client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/integration"
)

# ---- 05: corruption recovery -----------------------------------
add_executable(05-corruption-recovery 05-corruption-recovery.cpp)
target_link_libraries(05-corruption-recovery PRIVATE keydrop)
set_target_properties(05-corruption-recovery PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/integration"
)

# ---- Meta-target: build all examples ---------------------------
add_custom_target(all_integration
    DEPENDS
        01-send-receive
        02-with-dictionary
        03-stream-batching
        04-tcp-server
        04-tcp-client
        05-corruption-recovery
    COMMENT "Building all Keydrop integration examples"
)
