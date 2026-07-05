cmake_minimum_required(VERSION 3.13)

project(PicoSPIBridgeTests C)

option(BRIDGE_ENABLE_COVERAGE "Enable host test coverage instrumentation" OFF)

enable_testing()

add_executable(bridge_ring_test
    bridge_ring_test.c
    ../src/bridge_ring.c
)

target_include_directories(bridge_ring_test
    PRIVATE
        ../src
)

if (BRIDGE_ENABLE_COVERAGE)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(bridge_ring_test PRIVATE -O0 -g --coverage)
        target_link_options(bridge_ring_test PRIVATE --coverage)
    else ()
        message(FATAL_ERROR "Coverage is only supported with GCC or Clang compilers.")
    endif ()
endif ()

add_test(NAME bridge_ring_test COMMAND bridge_ring_test)