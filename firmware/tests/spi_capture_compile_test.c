#include <assert.h>

#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "spi_mosi_sniffer.pio.h"

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

int main(void) {
    bridge_ring_t ring;

    mock_gpio_values[PICO_SPI_BRIDGE_CS_PIN] = true;
    bridge_ring_init(&ring);
    spi_capture_init(&(spi_capture_config_t){.ring = &ring});
    assert(mock_spi_mosi_sniffer_init_calls == 1u);
    assert(mock_spi_mosi_sniffer_recovery_init_calls == 1u);
    spi_capture_poll();
    return 0;
}