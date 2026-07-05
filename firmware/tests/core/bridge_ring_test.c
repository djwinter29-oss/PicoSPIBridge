#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "bridge_ring.h"

static void test_empty_read(void) {
    bridge_ring_t ring;
    uint8_t output[3] = {0xaa, 0xbb, 0xcc};

    bridge_ring_init(&ring);

    assert(bridge_ring_read(&ring, output, 3) == 0);
    assert(output[0] == 0xaa);
    assert(output[1] == 0xbb);
    assert(output[2] == 0xcc);
}

static void test_round_trip(void) {
    bridge_ring_t ring;
    uint8_t input[] = {0x10, 0x20, 0x30, 0x40};
    uint8_t output[4] = {0};

    bridge_ring_init(&ring);

    assert(bridge_ring_write(&ring, input, 4) == 4);
    assert(bridge_ring_read(&ring, output, 4) == 4);
    assert(output[0] == 0x10);
    assert(output[1] == 0x20);
    assert(output[2] == 0x30);
    assert(output[3] == 0x40);
}

static void test_partial_read(void) {
    bridge_ring_t ring;
    uint8_t input[] = {0x01, 0x02, 0x03};
    uint8_t output[5] = {0};

    bridge_ring_init(&ring);

    assert(bridge_ring_write(&ring, input, 3) == 3);
    assert(bridge_ring_read(&ring, output, 5) == 3);
    assert(output[0] == 0x01);
    assert(output[1] == 0x02);
    assert(output[2] == 0x03);
}

static void test_partial_write_when_near_full(void) {
    bridge_ring_t ring;
    uint8_t fill = 0x7e;
    uint8_t input[] = {0x11, 0x22, 0x33, 0x44};
    size_t expected_capacity = BRIDGE_RING_SIZE - 1u;
    size_t index;

    bridge_ring_init(&ring);

    for (index = 0; index < expected_capacity - 2u; ++index) {
        assert(bridge_ring_write(&ring, &fill, 1) == 1);
    }

    assert(bridge_ring_write(&ring, input, 4) == 2);
}

static void test_fifo_order_across_wrap(void) {
    bridge_ring_t ring;
    uint8_t input_a[] = {0xa1, 0xa2, 0xa3};
    uint8_t input_b[] = {0xb1, 0xb2, 0xb3, 0xb4};
    uint8_t output[7] = {0};
    size_t index;
    size_t expected_capacity = BRIDGE_RING_SIZE - 1u;

    bridge_ring_init(&ring);

    for (index = 0; index < expected_capacity - 3u; ++index) {
        uint8_t value = (uint8_t)index;
        assert(bridge_ring_write(&ring, &value, 1) == 1);
    }

    for (index = 0; index < expected_capacity - 3u; ++index) {
        uint8_t sink = 0;
        assert(bridge_ring_read(&ring, &sink, 1) == 1);
        assert(sink == (uint8_t)index);
    }

    assert(bridge_ring_write(&ring, input_a, 3) == 3);
    assert(bridge_ring_write(&ring, input_b, 4) == 4);
    assert(bridge_ring_read(&ring, output, 7) == 7);

    assert(output[0] == 0xa1);
    assert(output[1] == 0xa2);
    assert(output[2] == 0xa3);
    assert(output[3] == 0xb1);
    assert(output[4] == 0xb2);
    assert(output[5] == 0xb3);
    assert(output[6] == 0xb4);
}

static void test_zero_length_operations(void) {
    bridge_ring_t ring;
    uint8_t byte = 0x55;

    bridge_ring_init(&ring);

    assert(bridge_ring_write(&ring, &byte, 0) == 0);
    assert(bridge_ring_read(&ring, &byte, 0) == 0);
    assert(ring.read_index == 0u);
    assert(ring.write_index == 0u);
}

static void test_wraparound(void) {
    bridge_ring_t ring;
    uint8_t source = 0x5a;
    uint8_t sink = 0;
    size_t expected_capacity = BRIDGE_RING_SIZE - 1u;
    size_t index;

    bridge_ring_init(&ring);

    for (index = 0; index < expected_capacity; ++index) {
        assert(bridge_ring_write(&ring, &source, 1) == 1);
        assert(bridge_ring_read(&ring, &sink, 1) == 1);
        assert(sink == source);
    }

    for (index = 0; index < expected_capacity; ++index) {
        assert(bridge_ring_write(&ring, &source, 1) == 1);
    }

    assert(bridge_ring_write(&ring, &source, 1) == 0);
}

int main(void) {
    test_empty_read();
    test_round_trip();
    test_partial_read();
    test_partial_write_when_near_full();
    test_fifo_order_across_wrap();
    test_zero_length_operations();
    test_wraparound();
    return 0;
}