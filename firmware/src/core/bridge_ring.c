#include <string.h>

#include "bridge_ring.h"

_Static_assert((BRIDGE_RING_SIZE & (BRIDGE_RING_SIZE - 1u)) == 0u, "BRIDGE_RING_SIZE must be a power of two");

static uint32_t bridge_ring_next(uint32_t index) {
    return (index + 1u) & (BRIDGE_RING_SIZE - 1u);
}

void bridge_ring_init(bridge_ring_t *ring) {
    memset(ring, 0, sizeof(*ring));
}

size_t bridge_ring_write(bridge_ring_t *ring, const uint8_t *source, size_t count) {
    size_t written = 0;

    while (written < count) {
        uint32_t next = bridge_ring_next(ring->write_index);

        if (next == ring->read_index) {
            break;
        }

        ring->storage[ring->write_index] = source[written];
        ring->write_index = next;
        ++written;
    }

    return written;
}

size_t bridge_ring_read(bridge_ring_t *ring, uint8_t *destination, size_t count) {
    size_t read = 0;

    while ((read < count) && (ring->read_index != ring->write_index)) {
        destination[read] = ring->storage[ring->read_index];
        ring->read_index = bridge_ring_next(ring->read_index);
        ++read;
    }

    return read;
}
