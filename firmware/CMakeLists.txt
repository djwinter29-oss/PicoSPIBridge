cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(PicoSPIBridge C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(pico_spi_bridge
    src/main.c
    src/bridge_ring.c
    src/capture/spi_capture.c
    src/capture/spi_mosi_sniffer.pio
    src/usb_stream.c
)

pico_generate_pio_header(
    pico_spi_bridge
    ${CMAKE_CURRENT_LIST_DIR}/src/capture/spi_mosi_sniffer.pio
)

target_include_directories(pico_spi_bridge
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
        ${CMAKE_CURRENT_LIST_DIR}/src/capture
)

target_link_libraries(pico_spi_bridge
    PRIVATE
        pico_stdlib
        hardware_dma
        hardware_irq
        hardware_pio
        tinyusb_device
        tinyusb_board
)

pico_enable_stdio_uart(pico_spi_bridge 0)
pico_enable_stdio_usb(pico_spi_bridge 0)

pico_add_extra_outputs(pico_spi_bridge)