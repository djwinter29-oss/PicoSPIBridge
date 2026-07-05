#include <assert.h>
#include <string.h>

#include "bridge_ring.h"

_Static_assert((BRIDGE_RING_SIZE & (BRIDGE_RING_SIZE - 1u)) == 0u, "BRIDGE_RING_SIZE must be a power of two");
_Static_assert((BRIDGE_RING_SIZE % 32u) == 0u, "BRIDGE_RING_SIZE must be divisible by 32");

static uint32_t bridge_ring_boundary_word_index(uint32_t index) {
    return index >> 5;
}

static uint32_t bridge_ring_boundary_mask(uint32_t index) {
    return 1u << (index & 31u);
}

static bool bridge_ring_boundary_is_set(const bridge_ring_t *ring, uint32_t index) {
    return (ring->usb_flush_boundary_bits[bridge_ring_boundary_word_index(index)] & bridge_ring_boundary_mask(index)) != 0u;
}

static void bridge_ring_boundary_set(bridge_ring_t *ring, uint32_t index) {
    ring->usb_flush_boundary_bits[bridge_ring_boundary_word_index(index)] |= bridge_ring_boundary_mask(index);
}

static void bridge_ring_boundary_clear(bridge_ring_t *ring, uint32_t index) {
    ring->usb_flush_boundary_bits[bridge_ring_boundary_word_index(index)] &= ~bridge_ring_boundary_mask(index);
}

static uint32_t bridge_ring_popcount32(uint32_t value) {
    uint32_t count = 0u;

    while (value != 0u) {
        value &= value - 1u;
        count += 1u;
    }

    return count;
}

static uint32_t bridge_ring_boundary_range_mask(uint32_t start_bit, uint32_t bit_count) {
    if (bit_count >= 32u) {
        return 0xffffffffu;
    }

    return ((1u << bit_count) - 1u) << start_bit;
}

static uint32_t bridge_ring_clear_boundary_segment(bridge_ring_t *ring, uint32_t start_index, uint32_t count) {
    uint32_t cleared = 0u;
    uint32_t current = start_index;
    uint32_t remaining = count;

    while (remaining != 0u) {
        uint32_t word_index = bridge_ring_boundary_word_index(current);
        uint32_t bit_index = current & 31u;
        uint32_t chunk_bits = 32u - bit_index;
        uint32_t mask;
        uint32_t matched;

        if (chunk_bits > remaining) {
            chunk_bits = remaining;
        }

        mask = bridge_ring_boundary_range_mask(bit_index, chunk_bits);
        matched = ring->usb_flush_boundary_bits[word_index] & mask;
        if (matched != 0u) {
            ring->usb_flush_boundary_bits[word_index] &= ~mask;
            cleared += bridge_ring_popcount32(matched);
        }

        current = (current + chunk_bits) & (BRIDGE_RING_SIZE - 1u);
        remaining -= chunk_bits;
    }

    return cleared;
}

static bool bridge_ring_clear_consumed_boundaries(bridge_ring_t *ring, uint32_t start_index, size_t count) {
    uint32_t cleared = 0u;

    if (count == 0u) {
        return false;
    }

    if ((start_index + (uint32_t)count) <= BRIDGE_RING_SIZE) {
        cleared = bridge_ring_clear_boundary_segment(ring, start_index, (uint32_t)count);
    } else {
        uint32_t first_count = BRIDGE_RING_SIZE - start_index;
        cleared = bridge_ring_clear_boundary_segment(ring, start_index, first_count);
        cleared += bridge_ring_clear_boundary_segment(ring, 0u, (uint32_t)count - first_count);
    }

    if (cleared != 0u) {
        ring->usb_flush_boundary_count -= cleared;
        return true;
    }

    return false;
}

static size_t bridge_ring_used(const bridge_ring_t *ring) {
    return (size_t)((ring->write_index - ring->read_index) & (BRIDGE_RING_SIZE - 1u));
}

static size_t bridge_ring_free(const bridge_ring_t *ring) {
    return (BRIDGE_RING_SIZE - 1u) - bridge_ring_used(ring);
}

static void bridge_ring_update_high_water_mark(bridge_ring_t *ring) {
    uint32_t used = (uint32_t)bridge_ring_used(ring);

    if (used > ring->stats.high_water_mark) {
        ring->stats.high_water_mark = used;
    }
}

void bridge_ring_init(bridge_ring_t *ring) {
    memset(ring, 0, sizeof(*ring));
}

size_t bridge_ring_write(bridge_ring_t *ring, const uint8_t *source, size_t count) {
    size_t written;
    size_t first_chunk;
    size_t second_chunk;

    written = count;
    if (written > bridge_ring_free(ring)) {
        written = bridge_ring_free(ring);
    }

    first_chunk = BRIDGE_RING_SIZE - ring->write_index;
    if (first_chunk > written) {
        first_chunk = written;
    }

    memcpy(&ring->storage[ring->write_index], source, first_chunk);

    second_chunk = written - first_chunk;
    if (second_chunk != 0u) {
        memcpy(ring->storage, &source[first_chunk], second_chunk);
    }

    ring->write_index = (uint32_t)((ring->write_index + written) & (BRIDGE_RING_SIZE - 1u));
    bridge_ring_update_high_water_mark(ring);

    return written;
}

size_t bridge_ring_read(bridge_ring_t *ring, uint8_t *destination, size_t count) {
    size_t read;
    size_t first_chunk;
    size_t second_chunk;

    read = count;
    if (read > bridge_ring_used(ring)) {
        read = bridge_ring_used(ring);
    }

    first_chunk = BRIDGE_RING_SIZE - ring->read_index;
    if (first_chunk > read) {
        first_chunk = read;
    }

    memcpy(destination, &ring->storage[ring->read_index], first_chunk);

    second_chunk = read - first_chunk;
    if (second_chunk != 0u) {
        memcpy(&destination[first_chunk], ring->storage, second_chunk);
    }

    ring->usb_flush_boundary_reached = bridge_ring_clear_consumed_boundaries(ring, ring->read_index, read);
    ring->read_index = (uint32_t)((ring->read_index + read) & (BRIDGE_RING_SIZE - 1u));

    return read;
}

size_t bridge_ring_peek_contiguous(const bridge_ring_t *ring, const uint8_t **source) {
    size_t used = bridge_ring_used(ring);
    size_t contiguous = BRIDGE_RING_SIZE - ring->read_index;

    if (source != NULL) {
        *source = &ring->storage[ring->read_index];
    }

    if (contiguous > used) {
        contiguous = used;
    }

    return contiguous;
}

void bridge_ring_produce(bridge_ring_t *ring, size_t count) {
    size_t free = bridge_ring_free(ring);

    if (count > free) {
        count = free;
    }

    ring->write_index = (uint32_t)((ring->write_index + count) & (BRIDGE_RING_SIZE - 1u));
    bridge_ring_update_high_water_mark(ring);
}

bool bridge_ring_publish(bridge_ring_t *ring, size_t count) {
    size_t free = bridge_ring_free(ring);

    if (count > free) {
        ring->stats.publish_invariant_failures += 1u;
        ring->dropped_bytes += (uint32_t)count;
        return false;
    }

    ring->write_index = (uint32_t)((ring->write_index + count) & (BRIDGE_RING_SIZE - 1u));
    bridge_ring_update_high_water_mark(ring);
    return true;
}

void bridge_ring_consume(bridge_ring_t *ring, size_t count) {
    size_t used = bridge_ring_used(ring);

    if (count > used) {
        count = used;
    }

    ring->usb_flush_boundary_reached = bridge_ring_clear_consumed_boundaries(ring, ring->read_index, count);
    ring->read_index = (uint32_t)((ring->read_index + count) & (BRIDGE_RING_SIZE - 1u));
}

void bridge_ring_note_usb_flush_boundary(bridge_ring_t *ring) {
    uint32_t boundary_index;

    if (bridge_ring_used(ring) == 0u) {
        return;
    }

    boundary_index = (ring->write_index - 1u) & (BRIDGE_RING_SIZE - 1u);
    if (!bridge_ring_boundary_is_set(ring, boundary_index)) {
        bridge_ring_boundary_set(ring, boundary_index);
        ring->usb_flush_boundary_count += 1u;
    }
}

bool bridge_ring_consume_reached_usb_flush_boundary(bridge_ring_t *ring) {
    bool boundary_reached = ring->usb_flush_boundary_reached;

    ring->usb_flush_boundary_reached = false;
    return boundary_reached;
}