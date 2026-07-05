#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "tusb.h"

#include "bridge_config.h"
#include "bridge_ring.h"
#include "usb_stream.h"

static bool stub_ready;
static bool stub_connected;
static uint32_t stub_available;
static uint32_t stub_available_calls;
static uint32_t stub_write_limit;
static uint32_t stub_write_calls;
static uint32_t stub_flush_calls;
static uint8_t stub_last_write[BRIDGE_USB_CHUNK_SIZE];
static uint32_t stub_last_write_size;

bool tud_ready(void) {
    return stub_ready;
}

bool tud_cdc_n_connected(uint8_t itf) {
    (void)itf;
    return stub_connected;
}

uint32_t tud_cdc_n_write_available(uint8_t itf) {
    (void)itf;
    stub_available_calls += 1u;
    return stub_available;
}

uint32_t tud_cdc_n_write(uint8_t itf, const void *buffer, uint32_t size) {
    uint32_t written = size;

    (void)itf;

    if (written > stub_write_limit) {
        written = stub_write_limit;
    }

    memcpy(stub_last_write, buffer, written);
    stub_last_write_size = written;
    stub_write_calls += 1u;
    return written;
}

void tud_cdc_n_write_flush(uint8_t itf) {
    (void)itf;
    stub_flush_calls += 1u;
}

uint32_t tud_cdc_n_available(uint8_t itf) {
    (void)itf;
    return 0u;
}

uint32_t tud_cdc_n_read(uint8_t itf, void *buffer, uint32_t size) {
    (void)itf;
    (void)buffer;
    (void)size;
    return 0u;
}

static void reset_usb_stub(void) {
    stub_ready = true;
    stub_connected = true;
    stub_available = BRIDGE_USB_CHUNK_SIZE;
    stub_available_calls = 0u;
    stub_write_limit = BRIDGE_USB_CHUNK_SIZE;
    stub_write_calls = 0u;
    stub_flush_calls = 0u;
    stub_last_write_size = 0u;
    memset(stub_last_write, 0, sizeof(stub_last_write));
}

static void fill_ring(bridge_ring_t *ring, uint32_t count) {
    uint32_t index;

    for (index = 0u; index < count; ++index) {
        uint8_t value = (uint8_t)index;
        assert(bridge_ring_write(ring, &value, 1u) == 1u);
    }
}

static void test_full_packet_flushes_when_batch_drains_ring(void) {
    bridge_ring_t ring;

    bridge_ring_init(&ring);
    reset_usb_stub();
    fill_ring(&ring, BRIDGE_USB_PACKET_SIZE);

    usb_stream_poll(&ring);

    assert(stub_write_calls == 1u);
    assert(stub_available_calls == 1u);
    assert(stub_flush_calls == 0u);
    assert(stub_last_write_size == BRIDGE_USB_PACKET_SIZE);
    assert(ring.stats.usb_write_calls == 1u);
    assert(ring.stats.usb_bytes_written == BRIDGE_USB_PACKET_SIZE);
    assert(ring.stats.usb_flush_calls == 0u);
    assert(ring.read_index == ring.write_index);
}

static void test_short_tail_flushes(void) {
    bridge_ring_t ring;

    bridge_ring_init(&ring);
    reset_usb_stub();
    fill_ring(&ring, 10u);

    usb_stream_poll(&ring);

    assert(stub_write_calls == 1u);
    assert(stub_available_calls == 1u);
    assert(stub_flush_calls == 1u);
    assert(stub_last_write_size == 10u);
    assert(ring.stats.usb_write_calls == 1u);
    assert(ring.stats.usb_bytes_written == 10u);
    assert(ring.stats.usb_flush_calls == 1u);
}

static void test_full_tx_budget_flushes_during_continuous_stream(void) {
    bridge_ring_t ring;

    bridge_ring_init(&ring);
    reset_usb_stub();
    stub_available = BRIDGE_USB_CHUNK_SIZE * 4u;
    fill_ring(&ring, BRIDGE_USB_CHUNK_SIZE * 3u);

    usb_stream_poll(&ring);

    assert(stub_write_calls == 2u);
    assert(stub_available_calls == 1u);
    assert(stub_flush_calls == 0u);
    assert(stub_last_write_size == BRIDGE_USB_CHUNK_SIZE);
    assert(ring.stats.usb_write_calls == 2u);
    assert(ring.stats.usb_bytes_written == (BRIDGE_USB_CHUNK_SIZE * 2u));
    assert(ring.stats.usb_flush_calls == 0u);
    assert(((ring.write_index - ring.read_index) & (BRIDGE_RING_SIZE - 1u)) == BRIDGE_USB_CHUNK_SIZE);
}

static void test_disconnected_host_drops_buffered_backlog(void) {
    bridge_ring_t ring;

    bridge_ring_init(&ring);
    reset_usb_stub();
    stub_connected = false;
    fill_ring(&ring, 100u);
    bridge_ring_note_usb_flush_boundary(&ring);

    usb_stream_poll(&ring);

    assert(stub_available_calls == 0u);
    assert(stub_write_calls == 0u);
    assert(stub_flush_calls == 0u);
    assert(ring.dropped_bytes == 100u);
    assert(ring.read_index == ring.write_index);
    assert(ring.usb_flush_pending_count == 0u);
}

static void test_pending_boundary_flushes_when_boundary_bytes_drain(void) {
    bridge_ring_t ring;

    bridge_ring_init(&ring);
    reset_usb_stub();
    fill_ring(&ring, BRIDGE_USB_CHUNK_SIZE * 2u);
    bridge_ring_note_usb_flush_boundary(&ring);

    usb_stream_poll(&ring);

    assert(stub_write_calls == 1u);
    assert(stub_available_calls == 1u);
    assert(stub_flush_calls == 0u);
    assert(stub_last_write_size == BRIDGE_USB_CHUNK_SIZE);
    assert(ring.stats.usb_write_calls == 1u);
    assert(ring.stats.usb_bytes_written == BRIDGE_USB_CHUNK_SIZE);
    assert(ring.stats.usb_flush_calls == 0u);
    assert(((ring.write_index - ring.read_index) & (BRIDGE_RING_SIZE - 1u)) == BRIDGE_USB_CHUNK_SIZE);

    usb_stream_poll(&ring);

    assert(stub_write_calls == 2u);
    assert(stub_available_calls == 2u);
    assert(stub_flush_calls == 1u);
    assert(stub_last_write_size == BRIDGE_USB_CHUNK_SIZE);
    assert(ring.stats.usb_write_calls == 2u);
    assert(ring.stats.usb_bytes_written == (BRIDGE_USB_CHUNK_SIZE * 2u));
    assert(ring.stats.usb_flush_calls == 1u);
    assert(ring.read_index == ring.write_index);
}

static void test_multiple_pending_boundaries_flush_in_order(void) {
    bridge_ring_t ring;

    bridge_ring_init(&ring);
    reset_usb_stub();
    fill_ring(&ring, BRIDGE_USB_CHUNK_SIZE);
    bridge_ring_note_usb_flush_boundary(&ring);
    fill_ring(&ring, BRIDGE_USB_CHUNK_SIZE);
    bridge_ring_note_usb_flush_boundary(&ring);

    usb_stream_poll(&ring);

    assert(stub_write_calls == 1u);
    assert(stub_flush_calls == 1u);
    assert(ring.usb_flush_pending_count == 1u);
    assert(ring.usb_flush_pending_bytes[0] == BRIDGE_USB_CHUNK_SIZE);
    assert(((ring.write_index - ring.read_index) & (BRIDGE_RING_SIZE - 1u)) == BRIDGE_USB_CHUNK_SIZE);

    usb_stream_poll(&ring);

    assert(stub_write_calls == 2u);
    assert(stub_flush_calls == 2u);
    assert(ring.usb_flush_pending_count == 0u);
    assert(ring.read_index == ring.write_index);
}

static void test_boundary_queue_saturation_keeps_first_coalesced_flush_bounded(void) {
    bridge_ring_t ring;

    bridge_ring_init(&ring);
    reset_usb_stub();
    stub_available = BRIDGE_USB_PACKET_SIZE;

    for (uint32_t index = 0u; index < (BRIDGE_USB_FLUSH_BOUNDARY_SLOTS + 1u); ++index) {
        fill_ring(&ring, BRIDGE_USB_PACKET_SIZE);
        bridge_ring_note_usb_flush_boundary(&ring);
    }

    assert(ring.usb_flush_pending_count == BRIDGE_USB_FLUSH_BOUNDARY_SLOTS);
    assert(ring.usb_flush_coalesced_bytes == ((BRIDGE_USB_FLUSH_BOUNDARY_SLOTS + 1u) * BRIDGE_USB_PACKET_SIZE));
    assert(ring.usb_flush_deferred_coalesced_bytes == 0u);
    assert(ring.stats.usb_flush_boundary_overflows == 1u);

    usb_stream_poll(&ring);

    assert(stub_write_calls == 1u);
    assert(stub_flush_calls == 1u);
    assert(ring.usb_flush_pending_count == (BRIDGE_USB_FLUSH_BOUNDARY_SLOTS - 1u));
    assert(ring.usb_flush_coalesced_bytes == (BRIDGE_USB_FLUSH_BOUNDARY_SLOTS * BRIDGE_USB_PACKET_SIZE));

    fill_ring(&ring, BRIDGE_USB_PACKET_SIZE);
    bridge_ring_note_usb_flush_boundary(&ring);
    fill_ring(&ring, BRIDGE_USB_PACKET_SIZE);
    bridge_ring_note_usb_flush_boundary(&ring);
    fill_ring(&ring, BRIDGE_USB_PACKET_SIZE);
    bridge_ring_note_usb_flush_boundary(&ring);

    assert(ring.usb_flush_pending_count == BRIDGE_USB_FLUSH_BOUNDARY_SLOTS);
    assert(ring.usb_flush_coalesced_bytes == (BRIDGE_USB_FLUSH_BOUNDARY_SLOTS * BRIDGE_USB_PACKET_SIZE));
    assert(ring.usb_flush_deferred_coalesced_bytes == ((BRIDGE_USB_FLUSH_BOUNDARY_SLOTS + 2u) * BRIDGE_USB_PACKET_SIZE));
    assert(ring.stats.usb_flush_boundary_overflows == 3u);

    usb_stream_poll(&ring);
    assert(stub_flush_calls == 2u);

    usb_stream_poll(&ring);
    assert(stub_flush_calls == 3u);

    usb_stream_poll(&ring);
    assert(stub_flush_calls == 4u);

    usb_stream_poll(&ring);

    assert(stub_write_calls == 5u);
    assert(stub_flush_calls == 5u);
    assert(ring.usb_flush_coalesced_bytes == (2u * BRIDGE_USB_PACKET_SIZE));
    assert(ring.usb_flush_deferred_coalesced_bytes == 0u);

    usb_stream_poll(&ring);

    assert(stub_write_calls == 6u);
    assert(stub_flush_calls == 6u);
    assert(ring.usb_flush_coalesced_bytes == BRIDGE_USB_PACKET_SIZE);

    usb_stream_poll(&ring);

    assert(stub_write_calls == 7u);
    assert(stub_flush_calls == 7u);
    assert(ring.usb_flush_coalesced_bytes == 0u);
    assert(((ring.write_index - ring.read_index) & (BRIDGE_RING_SIZE - 1u)) == BRIDGE_USB_PACKET_SIZE);

    usb_stream_poll(&ring);

    assert(stub_write_calls == 8u);
    assert(stub_flush_calls == 8u);
    assert(ring.usb_flush_force_on_write == false);
    assert(ring.read_index == ring.write_index);
}

static void test_boundary_tracking_exhaustion_falls_back_to_forced_flushes(void) {
    bridge_ring_t ring;

    bridge_ring_init(&ring);
    reset_usb_stub();
    stub_available = BRIDGE_USB_PACKET_SIZE;

    for (uint32_t index = 0u; index < (BRIDGE_USB_FLUSH_BOUNDARY_SLOTS + 4u); ++index) {
        fill_ring(&ring, BRIDGE_USB_PACKET_SIZE);
        bridge_ring_note_usb_flush_boundary(&ring);
    }

    assert(ring.usb_flush_pending_count == BRIDGE_USB_FLUSH_BOUNDARY_SLOTS);
    assert(ring.usb_flush_coalesced_bytes == ((BRIDGE_USB_FLUSH_BOUNDARY_SLOTS + 1u) * BRIDGE_USB_PACKET_SIZE));
    assert(ring.usb_flush_deferred_coalesced_bytes == ((BRIDGE_USB_FLUSH_BOUNDARY_SLOTS + 2u) * BRIDGE_USB_PACKET_SIZE));
    assert(ring.usb_flush_force_on_write == true);
    assert(ring.stats.usb_flush_boundary_overflows == 4u);

    for (uint32_t index = 0u; index < (BRIDGE_USB_FLUSH_BOUNDARY_SLOTS + 4u); ++index) {
        usb_stream_poll(&ring);
        assert(stub_write_calls == (index + 1u));
        assert(stub_flush_calls == (index + 1u));
    }

    assert(ring.usb_flush_pending_count == 0u);
    assert(ring.usb_flush_coalesced_bytes == 0u);
    assert(ring.usb_flush_deferred_coalesced_bytes == 0u);
    assert(ring.usb_flush_force_on_write == false);
    assert(ring.read_index == ring.write_index);
}

static void test_partial_write_flushes_short_tail_and_keeps_remaining_data(void) {
    bridge_ring_t ring;

    bridge_ring_init(&ring);
    reset_usb_stub();
    stub_write_limit = 32u;
    fill_ring(&ring, 100u);

    usb_stream_poll(&ring);

    assert(stub_write_calls == 1u);
    assert(stub_available_calls == 1u);
    assert(stub_flush_calls == 1u);
    assert(stub_last_write_size == 32u);
    assert(ring.stats.usb_write_calls == 1u);
    assert(ring.stats.usb_bytes_written == 32u);
    assert(ring.stats.usb_flush_calls == 1u);
    assert(((ring.write_index - ring.read_index) & (BRIDGE_RING_SIZE - 1u)) == 68u);
}

int main(void) {
    test_full_packet_flushes_when_batch_drains_ring();
    test_short_tail_flushes();
    test_full_tx_budget_flushes_during_continuous_stream();
    test_disconnected_host_drops_buffered_backlog();
    test_pending_boundary_flushes_when_boundary_bytes_drain();
    test_multiple_pending_boundaries_flush_in_order();
    test_boundary_queue_saturation_keeps_first_coalesced_flush_bounded();
    test_boundary_tracking_exhaustion_falls_back_to_forced_flushes();
    test_partial_write_flushes_short_tail_and_keeps_remaining_data();
    return 0;
}