#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#include "bridge_config.h"
#include "bridge_ring.h"
#include "spi_capture.h"
#include "spi_mosi_sniffer.pio.h"

typedef struct {
    PIO pio;
    uint sm;
    uint dma_channel;
    bridge_ring_t *ring;
    uint8_t dma_block[BRIDGE_DMA_BLOCK_SIZE];
} spi_capture_state_t;

static spi_capture_state_t capture;

static void spi_capture_arm_dma(void) {
    dma_channel_set_write_addr(capture.dma_channel, capture.dma_block, false);
    dma_channel_set_trans_count(capture.dma_channel, BRIDGE_DMA_BLOCK_SIZE, true);
}

static void spi_capture_commit_bytes(size_t count) {
    size_t written = bridge_ring_write(capture.ring, capture.dma_block, count);
    capture.ring->dropped_bytes += (uint32_t)(count - written);
}

static void spi_capture_dma_irq_handler(void) {
    uint32_t dma_mask = 1u << capture.dma_channel;

    if ((dma_hw->ints0 & dma_mask) == 0u) {
        return;
    }

    dma_hw->ints0 = dma_mask;

    spi_capture_commit_bytes(BRIDGE_DMA_BLOCK_SIZE);
    spi_capture_arm_dma();
}

void spi_capture_init(const spi_capture_config_t *config) {
    uint offset;
    dma_channel_config dma_config;

    capture.ring = config->ring;
    capture.pio = pio0;
    capture.sm = (uint)pio_claim_unused_sm(capture.pio, true);

    pio_gpio_init(capture.pio, PICO_SPI_BRIDGE_SCK_PIN);
    pio_gpio_init(capture.pio, PICO_SPI_BRIDGE_MOSI_PIN);
    pio_gpio_init(capture.pio, PICO_SPI_BRIDGE_CS_PIN);
    gpio_set_dir(PICO_SPI_BRIDGE_SCK_PIN, GPIO_IN);
    gpio_set_dir(PICO_SPI_BRIDGE_MOSI_PIN, GPIO_IN);
    gpio_set_dir(PICO_SPI_BRIDGE_CS_PIN, GPIO_IN);
    gpio_disable_pulls(PICO_SPI_BRIDGE_SCK_PIN);
    gpio_disable_pulls(PICO_SPI_BRIDGE_MOSI_PIN);
    gpio_disable_pulls(PICO_SPI_BRIDGE_CS_PIN);

    offset = pio_add_program(capture.pio, &spi_mosi_sniffer_program);

    // ponytail: This keeps CS gating in the PIO sampler instead of adding a framed packet layer.
    // The ceiling is that the stream still carries raw bytes without explicit transfer boundary markers.
    // That is acceptable for a minimal bridge; add host-visible framing if transfer boundaries matter downstream.
    spi_mosi_sniffer_program_init(capture.pio, capture.sm, offset);

    capture.dma_channel = (uint)dma_claim_unused_channel(true);
    dma_config = dma_channel_get_default_config(capture.dma_channel);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_8);
    channel_config_set_read_increment(&dma_config, false);
    channel_config_set_write_increment(&dma_config, true);
    channel_config_set_dreq(&dma_config, pio_get_dreq(capture.pio, capture.sm, false));
    dma_channel_configure(
        capture.dma_channel,
        &dma_config,
        capture.dma_block,
        &capture.pio->rxf[capture.sm],
        BRIDGE_DMA_BLOCK_SIZE,
        false
    );

    dma_channel_set_irq0_enabled(capture.dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_0, spi_capture_dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // ponytail: A 64-byte DMA block cuts IRQ load enough to make 5 MHz SPI practical on RP2040.
    // The ceiling is that short transfers can sit in the active DMA block until CS releases.
    // That is acceptable here because the main loop flushes partial blocks on CS high.
    spi_capture_arm_dma();
}

void spi_capture_poll(void) {
    uint32_t remaining;
    size_t captured;

    if (!gpio_get(PICO_SPI_BRIDGE_CS_PIN)) {
        return;
    }

    if (!pio_sm_is_rx_fifo_empty(capture.pio, capture.sm)) {
        return;
    }

    remaining = dma_hw->ch[capture.dma_channel].transfer_count;
    if ((remaining == 0u) || (remaining == BRIDGE_DMA_BLOCK_SIZE)) {
        return;
    }

    dma_channel_abort(capture.dma_channel);
    remaining = dma_hw->ch[capture.dma_channel].transfer_count;
    captured = BRIDGE_DMA_BLOCK_SIZE - (size_t)remaining;

    if (captured != 0u) {
        spi_capture_commit_bytes(captured);
    }

    spi_capture_arm_dma();
}