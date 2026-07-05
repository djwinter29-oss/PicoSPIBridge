#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "spi_mosi_sniffer.pio.h"

#include "bridge_config.h"
#include "bridge_ring.h"
#include "spi_capture.h"

mock_dma_hw_t mock_dma_hw = {0};
mock_dma_hw_t *dma_hw = &mock_dma_hw;
unsigned int mock_dma_next_channel = 0u;
volatile void *mock_dma_write_addresses[16] = {0};
uint32_t mock_dma_configure_transfer_counts[16] = {0};
uint32_t mock_dma_abort_calls[16] = {0};
uint32_t mock_dma_last_abort_sequence[16] = {0};
void (*mock_dma_irq0_handler)(void) = 0;
mock_pio_hw_t mock_pio0_hw = {{0}};
PIO pio0 = &mock_pio0_hw;
uint32_t mock_pio_set_enabled_calls = 0u;
bool mock_pio_sm_enabled = false;
uint32_t mock_pio_last_disable_sequence = 0u;
uint32_t mock_pio_clear_fifos_calls = 0u;
bool mock_gpio_values[32] = {0};
gpio_irq_callback_t mock_gpio_irq_callback = 0;
uint32_t mock_spi_mosi_sniffer_init_calls = 0u;
uint32_t mock_spi_mosi_sniffer_recovery_init_calls = 0u;
uint32_t mock_spi_mosi_sniffer_last_init_sequence = 0u;
uint32_t mock_call_sequence = 0u;

static bool address_in_ring(const bridge_ring_t *ring, const volatile void *address) {
    const uintptr_t start = (uintptr_t)&ring->storage[0];
    const uintptr_t end = (uintptr_t)&ring->storage[BRIDGE_RING_SIZE];
    const uintptr_t value = (uintptr_t)address;

    return (value >= start) && (value < end);
}

int main(void) {
    bridge_ring_t ring;

    bridge_ring_init(&ring);
    ring.read_index = 1u;
    mock_gpio_values[PICO_SPI_BRIDGE_CS_PIN] = true;

    spi_capture_init(&(spi_capture_config_t){
        .ring = &ring,
    });

    assert(mock_dma_irq0_handler != 0);
    assert(!address_in_ring(&ring, mock_dma_write_addresses[0]));
    assert(!address_in_ring(&ring, mock_dma_write_addresses[1]));

    dma_hw->ch[0].transfer_count = BRIDGE_DMA_BLOCK_SIZE - 5u;
    spi_capture_poll();

    assert(ring.dropped_bytes == 5u);
    assert(ring.stats.overflow_commits == 1u);

    dma_hw->ints0 = 1u << 0;
    dma_hw->ch[0].transfer_count = 0u;
    mock_dma_irq0_handler();

    assert(ring.dropped_bytes == BRIDGE_DMA_BLOCK_SIZE);
    assert(ring.stats.overflow_commits == 1u);

    return 0;
}