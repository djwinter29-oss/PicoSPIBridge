#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

#include "bridge_config.h"
#include "bridge_ring.h"
#include "capture_dma_plan.h"
#include "spi_capture.h"
#include "spi_mosi_sniffer.pio.h"

typedef struct {
    volatile uint32_t active_block_index;
    volatile uint32_t flushed_counts[2];
} spi_capture_shared_state_t;

typedef struct {
    uint8_t *destination;
    uint32_t transfer_count;
    bool drop_on_commit;
    bool overflow_counted;
    volatile bool ring_reserved;
    volatile bool dma_armed;
} spi_capture_dma_target_t;

typedef struct {
    PIO pio;
    uint sm;
    uint program_offset;
    uint dma_channels[2];
    bridge_ring_t *ring;
    spi_capture_shared_state_t shared;
    volatile bool recovery_active;
    volatile bool recovery_quiescing;
    volatile bool recovery_resume_pending;
    uint32_t reserved_write_index;
    spi_capture_dma_target_t dma_targets[2];
    uint8_t overflow_blocks[2][BRIDGE_DMA_BLOCK_SIZE];
} spi_capture_state_t;

static spi_capture_state_t capture;

typedef struct {
    uint block_index;
    uint dma_channel;
    uint32_t transfer_count;
    uint32_t flushed_count;
    uint32_t remaining_count;
} spi_capture_snapshot_t;

static void spi_capture_configure_dma_channel(uint block_index, bool trigger);
static bool spi_capture_recovery_boundary_ready(void);
static void spi_capture_restart_dma_after_recovery(void);
static bool spi_capture_publish_bytes_from_block(uint block_index, uint32_t count);
static void spi_capture_enter_recovery(void);

static void spi_capture_reset_sniffer_to_wait_cs_low(void) {
    spi_mosi_sniffer_program_init(capture.pio, capture.sm, capture.program_offset);
}

static void spi_capture_abort_dma_for_recovery(void) {
    uint block_index;

    for (block_index = 0u; block_index < 2u; ++block_index) {
        dma_channel_abort(capture.dma_channels[block_index]);
        capture.dma_targets[block_index].dma_armed = false;
        capture.dma_targets[block_index].drop_on_commit = true;
        capture.dma_targets[block_index].ring_reserved = false;
    }

    capture.shared.flushed_counts[0] = 0u;
    capture.shared.flushed_counts[1] = 0u;
    capture.recovery_quiescing = false;
    capture.recovery_resume_pending = true;
}

static void spi_capture_enter_recovery(void) {
    if (capture.recovery_active) {
        return;
    }

    capture.recovery_active = true;
    spi_capture_reset_sniffer_to_wait_cs_low();
    spi_capture_abort_dma_for_recovery();
}

static bool spi_capture_all_dma_idle(void) {
    return !capture.dma_targets[0].dma_armed && !capture.dma_targets[1].dma_armed;
}

static bool spi_capture_recovery_boundary_ready(void) {
    return capture.recovery_resume_pending
        && gpio_get(PICO_SPI_BRIDGE_CS_PIN)
        && spi_capture_all_dma_idle()
        && pio_sm_is_rx_fifo_empty(capture.pio, capture.sm);
}

static void spi_capture_restart_dma_after_recovery(void) {
    capture.recovery_active = false;
    capture.recovery_resume_pending = false;
    capture.reserved_write_index = capture.ring->write_index;
    capture.shared.active_block_index = 0u;
    capture.shared.flushed_counts[0] = 0u;
    capture.shared.flushed_counts[1] = 0u;

    // Reset the sniffer to its idle wait state so resumed capture starts on the next CS-low assertion.
    spi_capture_reset_sniffer_to_wait_cs_low();

    spi_capture_configure_dma_channel(0u, false);
    spi_capture_configure_dma_channel(1u, false);
    dma_start_channel_mask(1u << capture.dma_channels[capture.shared.active_block_index]);
}

static void spi_capture_try_finish_recovery(void) {
    uint32_t irq_state = save_and_disable_interrupts();

    if (spi_capture_recovery_boundary_ready()) {
        spi_capture_restart_dma_after_recovery();
    }

    restore_interrupts(irq_state);
}

static bool spi_capture_recovery_can_resume(void) {
    return !capture.dma_targets[0].ring_reserved
        && !capture.dma_targets[1].ring_reserved
        && spi_capture_all_dma_idle();
}

static void spi_capture_retire_dma_target(uint block_index) {
    capture.dma_targets[block_index].dma_armed = false;

    if (capture.dma_targets[block_index].ring_reserved) {
        capture.dma_targets[block_index].ring_reserved = false;
    }

    if (capture.recovery_active && !capture.dma_targets[0].ring_reserved && !capture.dma_targets[1].ring_reserved) {
        capture.recovery_quiescing = true;
    }

    if (capture.recovery_quiescing && spi_capture_recovery_can_resume()) {
        capture.reserved_write_index = capture.ring->write_index;
        capture.recovery_resume_pending = true;
        capture.recovery_quiescing = false;
    }
}

static void spi_capture_assign_dma_target(uint block_index) {

    if (capture.recovery_active) {
        capture.dma_targets[block_index].destination = capture.overflow_blocks[block_index];
        capture.dma_targets[block_index].transfer_count = BRIDGE_DMA_BLOCK_SIZE;
        capture.dma_targets[block_index].drop_on_commit = true;
        capture.dma_targets[block_index].overflow_counted = false;
        capture.dma_targets[block_index].ring_reserved = false;
        return;
    }

    capture_dma_plan_t plan = capture_dma_plan_target(capture.reserved_write_index, capture.ring->read_index);

    if (plan.drop_on_commit) {
        capture.dma_targets[block_index].destination = capture.overflow_blocks[block_index];
        capture.dma_targets[block_index].transfer_count = plan.transfer_count;
        capture.dma_targets[block_index].drop_on_commit = true;
        capture.dma_targets[block_index].overflow_counted = false;
        capture.dma_targets[block_index].ring_reserved = false;
        return;
    }

    capture.dma_targets[block_index].destination = &capture.ring->storage[capture.reserved_write_index];
    capture.dma_targets[block_index].transfer_count = plan.transfer_count;
    capture.dma_targets[block_index].drop_on_commit = false;
    capture.dma_targets[block_index].overflow_counted = false;
    capture.dma_targets[block_index].ring_reserved = true;
    capture.reserved_write_index = plan.next_reserved_write_index;
}

static void spi_capture_configure_dma_channel(uint block_index, bool trigger) {
    dma_channel_config dma_config = dma_channel_get_default_config(capture.dma_channels[block_index]);

    spi_capture_assign_dma_target(block_index);

    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_8);
    channel_config_set_read_increment(&dma_config, false);
    channel_config_set_write_increment(&dma_config, true);
    channel_config_set_dreq(&dma_config, pio_get_dreq(capture.pio, capture.sm, false));
    channel_config_set_chain_to(&dma_config, capture.dma_channels[block_index ^ 1u]);

    dma_channel_configure(
        capture.dma_channels[block_index],
        &dma_config,
        capture.dma_targets[block_index].destination,
        &capture.pio->rxf[capture.sm],
        capture.dma_targets[block_index].transfer_count,
        trigger
    );

    capture.dma_targets[block_index].dma_armed = true;
}

static bool spi_capture_publish_bytes_from_block(uint block_index, uint32_t count) {
    if (count == 0u) {
        return false;
    }

    if (capture.recovery_active) {
        capture.ring->dropped_bytes += count;
        return false;
    }

    if (capture.dma_targets[block_index].drop_on_commit) {
        if (capture_overflow_note_commit(&capture.dma_targets[block_index].overflow_counted)) {
            capture.ring->stats.overflow_commits += 1u;
        }
        capture.ring->dropped_bytes += count;
        return false;
    }

    if (!bridge_ring_publish(capture.ring, count)) {
        spi_capture_enter_recovery();
        return false;
    }

    return true;
}

static bool spi_capture_snapshot_active_dma_state(spi_capture_snapshot_t *snapshot, uint32_t *irq_state) {
    *irq_state = save_and_disable_interrupts();

    if (!gpio_get(PICO_SPI_BRIDGE_CS_PIN)) {
        restore_interrupts(*irq_state);
        return false;
    }

    if (!pio_sm_is_rx_fifo_empty(capture.pio, capture.sm)) {
        restore_interrupts(*irq_state);
        return false;
    }

    snapshot->block_index = capture.shared.active_block_index;
    snapshot->dma_channel = capture.dma_channels[snapshot->block_index];
    snapshot->transfer_count = capture.dma_targets[snapshot->block_index].transfer_count;
    snapshot->flushed_count = capture.shared.flushed_counts[snapshot->block_index];
    snapshot->remaining_count = dma_hw->ch[snapshot->dma_channel].transfer_count;

    if (!capture_poll_ready_bytes(snapshot->transfer_count, snapshot->flushed_count, snapshot->remaining_count, NULL)) {
        restore_interrupts(*irq_state);
        return false;
    }

    return true;
}

static void spi_capture_dma_irq_handler(void) {
    uint32_t pending = dma_hw->ints0;
    uint block_index;

    for (block_index = 0u; block_index < 2u; ++block_index) {
        uint32_t dma_mask = 1u << capture.dma_channels[block_index];
        size_t ready_count;

        if ((pending & dma_mask) == 0u) {
            continue;
        }

        dma_hw->ints0 = dma_mask;

        ready_count = capture_completion_ready_bytes(
            capture.dma_targets[block_index].transfer_count,
            capture.shared.flushed_counts[block_index]
        );
        if (ready_count != 0u) {
            spi_capture_publish_bytes_from_block(block_index, (uint32_t)ready_count);
        }

        spi_capture_retire_dma_target(block_index);
        capture.shared.flushed_counts[block_index] = 0u;
        capture.shared.active_block_index = block_index ^ 1u;
        if (!capture.recovery_quiescing && !capture.recovery_resume_pending) {
            spi_capture_configure_dma_channel(block_index, false);
        }
        capture.ring->stats.dma_rearm_count += 1u;
    }
}

void spi_capture_init(const spi_capture_config_t *config) {
    uint offset;

    capture.ring = config->ring;
    capture.pio = pio0;
    capture.sm = (uint)pio_claim_unused_sm(capture.pio, true);
    capture.recovery_active = false;
    capture.recovery_quiescing = false;
    capture.recovery_resume_pending = false;
    capture.reserved_write_index = capture.ring->write_index;

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
    capture.program_offset = offset;

    // ponytail: This keeps CS gating in the PIO sampler instead of adding a framed packet layer.
    // The ceiling is that the stream still carries raw bytes without explicit transfer boundary markers.
    // That is acceptable for a minimal bridge; add host-visible framing if transfer boundaries matter downstream.
    spi_capture_reset_sniffer_to_wait_cs_low();

    capture.dma_channels[0] = (uint)dma_claim_unused_channel(true);
    capture.dma_channels[1] = (uint)dma_claim_unused_channel(true);

    spi_capture_configure_dma_channel(0u, false);
    spi_capture_configure_dma_channel(1u, false);

    dma_channel_set_irq0_enabled(capture.dma_channels[0], true);
    dma_channel_set_irq0_enabled(capture.dma_channels[1], true);
    irq_set_exclusive_handler(DMA_IRQ_0, spi_capture_dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // ponytail: Two chained 4 KB DMA buffers spend more SRAM to reduce full-block recycle frequency.
    // The ceiling is that short transfers still require foreground partial-buffer flushes on CS high.
    // That is acceptable here; move framing into hardware if that foreground flush becomes the limit.
    capture.shared.active_block_index = 0u;
    capture.shared.flushed_counts[0] = 0u;
    capture.shared.flushed_counts[1] = 0u;
    dma_start_channel_mask(1u << capture.dma_channels[0]);
}

void spi_capture_poll(void) {
    uint32_t irq_state;
    spi_capture_snapshot_t snapshot;
    uint32_t captured;
    uint32_t ready_count;
    bool published;

    spi_capture_try_finish_recovery();

    if (!spi_capture_snapshot_active_dma_state(&snapshot, &irq_state)) {
        return;
    }

    if (!capture_poll_ready_bytes(snapshot.transfer_count, snapshot.flushed_count, snapshot.remaining_count, &ready_count)) {
        restore_interrupts(irq_state);
        return;
    }

    captured = snapshot.transfer_count - snapshot.remaining_count;

    // ponytail: Tail flushes still keep IRQs masked while publishing bytes so partial commits stay ordered with DMA completions.
    // The ceiling is extra interrupt latency during CS-high publish operations.
    // That is acceptable for now because the payload is already in the ring; move the consumer off-core if publish latency becomes the limit.
    published = spi_capture_publish_bytes_from_block(snapshot.block_index, ready_count);
    if (published) {
        bridge_ring_note_usb_flush_boundary(capture.ring);
    }
    capture.shared.flushed_counts[snapshot.block_index] = captured;
    restore_interrupts(irq_state);
}