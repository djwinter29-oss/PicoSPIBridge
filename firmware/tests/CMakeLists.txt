cmake_minimum_required(VERSION 3.13)

project(PicoSPIBridgeTests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(BRIDGE_ENABLE_COVERAGE "Enable host test coverage instrumentation" OFF)

enable_testing()

add_executable(bridge_ring_test
    bridge_ring_test.c
    ../src/bridge_ring.c
)

add_executable(capture_dma_plan_test
    capture_dma_plan_test.c
    ../src/capture/capture_dma_plan.c
)

add_executable(usb_stream_test
    usb_stream_test.c
    ../src/bridge_ring.c
    ../src/usb_stream.c
)

target_include_directories(bridge_ring_test
    PRIVATE
        ../src
)

target_include_directories(capture_dma_plan_test
    PRIVATE
        ../src
        ../src/capture
)

target_include_directories(usb_stream_test
    PRIVATE
        .
        ../src
)

if (BRIDGE_ENABLE_COVERAGE)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(bridge_ring_test PRIVATE -O0 -g --coverage)
        target_link_options(bridge_ring_test PRIVATE --coverage)
        target_compile_options(capture_dma_plan_test PRIVATE -O0 -g --coverage)
        target_link_options(capture_dma_plan_test PRIVATE --coverage)
        target_compile_options(usb_stream_test PRIVATE -O0 -g --coverage)
        target_link_options(usb_stream_test PRIVATE --coverage)
    else ()
        message(FATAL_ERROR "Coverage is only supported with GCC or Clang compilers.")
    endif ()
endif ()

add_test(NAME bridge_ring_test COMMAND bridge_ring_test)
add_test(NAME capture_dma_plan_test COMMAND capture_dma_plan_test)
add_test(NAME usb_stream_test COMMAND usb_stream_test)