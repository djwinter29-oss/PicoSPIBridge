#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"

#include "bridge_config.h"
#include "bridge_ring.h"
#include "spi_capture.h"

mock_dma_hw_t mock_dma_hw = {0};
mock_dma_hw_t *dma_hw = &mock_dma_hw;
unsigned int mock_dma_next_channel = 0u;
volatile void *mock_dma_write_addresses[16] = {0};
uint32_t mock_dma_configure_transfer_counts[16] = {0};
void (*mock_dma_irq0_handler)(void) = 0;
mock_pio_hw_t mock_pio0_hw = {{0}};
PIO pio0 = &mock_pio0_hw;

static bool address_in_ring(const bridge_ring_t *ring, const volatile void *address) {
    const uintptr_t start = (uintptr_t)&ring->storage[0];
    const uintptr_t end = (uintptr_t)&ring->storage[BRIDGE_RING_SIZE];
    const uintptr_t value = (uintptr_t)address;

    return (value >= start) && (value < end);
}

int main(void) {
    bridge_ring_t ring;

    bridge_ring_init(&ring);

    spi_capture_init(&(spi_capture_config_t){
        .ring = &ring,
    });

    assert(mock_dma_irq0_handler != 0);
    assert(mock_dma_configure_transfer_counts[0] == BRIDGE_DMA_BLOCK_SIZE);
    assert(mock_dma_configure_transfer_counts[1] == BRIDGE_DMA_BLOCK_SIZE);
    assert(address_in_ring(&ring, mock_dma_write_addresses[0]));
    assert(address_in_ring(&ring, mock_dma_write_addresses[1]));

    ring.read_index = 1u;
    dma_hw->ints0 = 1u << 0;
    dma_hw->ch[0].transfer_count = 0u;
    mock_dma_irq0_handler();

    assert(ring.stats.publish_invariant_failures == 1u);
    assert(!address_in_ring(&ring, mock_dma_write_addresses[0]));

    ring.read_index = (BRIDGE_DMA_BLOCK_SIZE * 2u) + 1u;
    dma_hw->ints0 = 1u << 1;
    dma_hw->ch[1].transfer_count = 0u;
    mock_dma_irq0_handler();

    assert(ring.write_index == BRIDGE_DMA_BLOCK_SIZE);
    assert(!address_in_ring(&ring, mock_dma_write_addresses[1]));

    dma_hw->ints0 = 1u << 0;
    dma_hw->ch[0].transfer_count = 0u;
    mock_dma_irq0_handler();

    assert(address_in_ring(&ring, mock_dma_write_addresses[0]));
    assert(!address_in_ring(&ring, mock_dma_write_addresses[1]));

    return 0;
}