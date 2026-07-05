#include "tusb.h"

#include "bridge_config.h"
#include "bridge_ring.h"
#include "usb_stream.h"

static bool usb_stream_has_short_packet_tail(uint32_t total_written) {
    return (total_written & (BRIDGE_USB_PACKET_SIZE - 1u)) != 0u;
}

void usb_stream_poll(bridge_ring_t *ring) {
    uint32_t available_budget;
    size_t initial_used;
    uint32_t total_written = 0u;
    bool should_flush = false;

    if (ring == NULL) {
        return;
    }

    if (!tud_ready()) {
        return;
    }

    available_budget = tud_cdc_n_write_available(0);
    if (available_budget == 0u) {
        return;
    }

    initial_used = (size_t)((ring->write_index - ring->read_index) & (BRIDGE_RING_SIZE - 1u));
    if (initial_used == 0u) {
        return;
    }

    while (true) {
        const uint8_t *chunk;
        size_t count;
        uint32_t write_size;
        uint32_t written;
        size_t remaining_used;

        if (available_budget == 0u) {
            should_flush = total_written != 0u;
            break;
        }

        count = available_budget;
        if (count > BRIDGE_USB_CHUNK_SIZE) {
            count = BRIDGE_USB_CHUNK_SIZE;
        }

        {
            size_t available_count = bridge_ring_peek_contiguous(ring, &chunk);
            if (count > available_count) {
                count = available_count;
            }
        }

        if (count == 0u) {
            break;
        }

        write_size = (uint32_t)count;
        written = tud_cdc_n_write(0, chunk, write_size);
        if (written != 0u) {
            ring->stats.usb_write_calls += 1u;
            ring->stats.usb_bytes_written += written;
            total_written += written;
            available_budget -= written;
        }
        bridge_ring_consume(ring, written);
        remaining_used = (size_t)((ring->write_index - ring->read_index) & (BRIDGE_RING_SIZE - 1u));

        if (written != write_size) {
            should_flush = total_written != 0u;
            break;
        }

        if (remaining_used == 0u) {
            should_flush = true;
            break;
        }
    }

    if (should_flush && (total_written != 0u)) {
        // ponytail: This assumes RP2040 full-speed CDC semantics where batching multiple 64-byte packets is cheaper than forcing every poll to flush.
        // The ceiling is that a different USB transport policy may want a more explicit latency target.
        // That is acceptable for this bridge; add a timed flush policy if host-visible latency becomes more important than bulk throughput.
        tud_cdc_n_write_flush(0);
        ring->stats.usb_flush_calls += 1u;
    }
}

void tud_cdc_rx_cb(uint8_t itf) {
    uint8_t sink[BRIDGE_USB_CHUNK_SIZE];

    while (tud_cdc_n_available(itf) != 0u) {
        (void)tud_cdc_n_read(itf, sink, sizeof(sink));
    }
}