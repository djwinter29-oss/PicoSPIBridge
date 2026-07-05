#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "tusb.h"

#include "bridge_config.h"
#include "bridge_ring.h"
#include "usb_stream.h"

static bool stub_ready;
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

static void test_full_packet_does_not_flush_on_empty(void) {
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
    test_full_packet_does_not_flush_on_empty();
    test_short_tail_flushes();
    test_partial_write_flushes_short_tail_and_keeps_remaining_data();
    return 0;
}