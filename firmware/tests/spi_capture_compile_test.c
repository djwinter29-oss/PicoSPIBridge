#include "hardware/dma.h"
#include "hardware/pio.h"

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

int main(void) {
    bridge_ring_t ring;

    bridge_ring_init(&ring);
    spi_capture_init(&(spi_capture_config_t){.ring = &ring});
    spi_capture_poll();
    return 0;
}