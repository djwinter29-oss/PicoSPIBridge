#include "tusb.h"

#include "bridge_config.h"
#include "bridge_ring.h"
#include "usb_stream.h"

#define USB_STREAM_MAX_WRITES_PER_POLL 2u

static bool usb_stream_has_short_packet_tail(uint32_t total_written) {
    return (total_written & (BRIDGE_USB_PACKET_SIZE - 1u)) != 0u;
}

static void usb_stream_drop_buffered_backlog(bridge_ring_t *ring) {
    uint32_t used = (uint32_t)((ring->write_index - ring->read_index) & (BRIDGE_RING_SIZE - 1u));

    if (used == 0u) {
        return;
    }

    ring->dropped_bytes += used;
    ring->read_index = ring->write_index;
    ring->usb_flush_pending_count = 0u;
    ring->usb_flush_coalesced_bytes = 0u;
    ring->usb_flush_deferred_coalesced_bytes = 0u;
    ring->usb_flush_force_on_write = false;
}

void usb_stream_poll(bridge_ring_t *ring) {
    uint32_t available_budget;
    size_t initial_used;
    uint32_t writes_remaining = USB_STREAM_MAX_WRITES_PER_POLL;
    const uint8_t *chunk;
    size_t count;
    uint32_t write_size;
    uint32_t total_written = 0u;
    uint32_t written;
    size_t remaining_used;
    bool should_flush = false;

    if (ring == NULL) {
        return;
    }

    if (!tud_ready()) {
        return;
    }

    if (!tud_cdc_n_connected(0)) {
        usb_stream_drop_buffered_backlog(ring);
        return;
    }

    initial_used = (size_t)((ring->write_index - ring->read_index) & (BRIDGE_RING_SIZE - 1u));

    available_budget = tud_cdc_n_write_available(0);
    if (available_budget == 0u) {
        return;
    }

    if (initial_used == 0u) {
        return;
    }

    while (writes_remaining != 0u) {
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

        if ((written != 0u) && bridge_ring_consume_reached_usb_flush_boundary(ring, written)) {
            should_flush = true;
            break;
        }

        if (written != write_size) {
            should_flush = total_written != 0u;
            break;
        }

        if (remaining_used == 0u) {
            should_flush = usb_stream_has_short_packet_tail(total_written);
            break;
        }

        if (available_budget == 0u) {
            break;
        }

        writes_remaining -= 1u;
    }

    if (should_flush && (total_written != 0u)) {
        // ponytail: This keeps each foreground drain pass bounded while still allowing a little batching when TinyUSB has room.
        // The ceiling is that a different transport or workload may want a different writes-per-poll budget.
        // That is acceptable here because the bridge is tuned experimentally; raise or lower the per-poll budget if hardware results move the best point.
        tud_cdc_n_write_flush(0);
        ring->stats.usb_flush_calls += 1u;
    }
}

void tud_cdc_rx_cb(uint8_t itf) {
    uint8_t sink[BRIDGE_USB_PACKET_SIZE];

    while (tud_cdc_n_available(itf) != 0u) {
        (void)tud_cdc_n_read(itf, sink, sizeof(sink));
    }
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
    (void)itf;
    (void)dtr;
    (void)rts;
}