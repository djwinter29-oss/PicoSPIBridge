#ifndef MOCK_SPI_MOSI_SNIFFER_PIO_H
#define MOCK_SPI_MOSI_SNIFFER_PIO_H

#include "hardware/pio.h"

#define PICO_SPI_BRIDGE_SCK_PIN 2u
#define PICO_SPI_BRIDGE_MOSI_PIN 3u
#define PICO_SPI_BRIDGE_MISO_PIN 4u
#define PICO_SPI_BRIDGE_CS_PIN 5u

static const pio_program_t spi_mosi_sniffer_program = {0};

extern uint32_t mock_spi_mosi_sniffer_init_calls;
extern uint32_t mock_spi_mosi_sniffer_last_init_sequence;
extern uint32_t mock_call_sequence;

static inline void spi_mosi_sniffer_program_init(PIO pio, uint sm, uint offset) {
    (void)pio;
    (void)sm;
    (void)offset;
    mock_call_sequence += 1u;
    mock_spi_mosi_sniffer_init_calls += 1u;
    mock_spi_mosi_sniffer_last_init_sequence = mock_call_sequence;
    pio_sm_set_enabled(pio, sm, true);
}

static inline void spi_mosi_sniffer_recovery_program_init(PIO pio, uint sm, uint offset) {
    spi_mosi_sniffer_program_init(pio, sm, offset);
}

#endif