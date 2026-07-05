#include <assert.h>
#include <string.h>

#include "bridge_ring.h"

_Static_assert((BRIDGE_RING_SIZE & (BRIDGE_RING_SIZE - 1u)) == 0u, "BRIDGE_RING_SIZE must be a power of two");

static uint32_t bridge_ring_next(uint32_t index) {
    return (index + 1u) & (BRIDGE_RING_SIZE - 1u);
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

void bridge_ring_publish(bridge_ring_t *ring, size_t count) {
    size_t free = bridge_ring_free(ring);

    if (count > free) {
        ring->stats.publish_invariant_failures += 1u;
        ring->dropped_bytes += (uint32_t)count;
        return;
    }

    ring->write_index = (uint32_t)((ring->write_index + count) & (BRIDGE_RING_SIZE - 1u));
    bridge_ring_update_high_water_mark(ring);
}

void bridge_ring_consume(bridge_ring_t *ring, size_t count) {
    size_t used = bridge_ring_used(ring);

    if (count > used) {
        count = used;
    }

    ring->read_index = (uint32_t)((ring->read_index + count) & (BRIDGE_RING_SIZE - 1u));
}