#include <assert.h>
#include <string.h>

#include "bridge_ring.h"

_Static_assert((BRIDGE_RING_SIZE & (BRIDGE_RING_SIZE - 1u)) == 0u, "BRIDGE_RING_SIZE must be a power of two");

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

    ring->read_index = (uint32_t)((ring->read_index + count) & (BRIDGE_RING_SIZE - 1u));
}

void bridge_ring_note_usb_flush_boundary(bridge_ring_t *ring) {
    uint32_t coalesced_bytes;
    uint32_t deferred_coalesced_bytes;
    uint32_t pending_count;
    size_t used;

    if (ring == NULL) {
        return;
    }

    used = bridge_ring_used(ring);
    if (used == 0u) {
        return;
    }

    pending_count = ring->usb_flush_pending_count;
    if (pending_count != 0u) {
        uint32_t last_pending = ring->usb_flush_pending_bytes[pending_count - 1u];

        if ((uint32_t)used <= last_pending) {
            return;
        }
    }

    coalesced_bytes = ring->usb_flush_coalesced_bytes;
    if ((coalesced_bytes != 0u) && ((uint32_t)used <= coalesced_bytes)) {
        return;
    }

    deferred_coalesced_bytes = ring->usb_flush_deferred_coalesced_bytes;
    if ((deferred_coalesced_bytes != 0u) && ((uint32_t)used <= deferred_coalesced_bytes)) {
        return;
    }

    if (pending_count >= BRIDGE_USB_FLUSH_BOUNDARY_SLOTS) {
        if (coalesced_bytes == 0u) {
            ring->usb_flush_coalesced_bytes = (uint32_t)used;
        } else if (deferred_coalesced_bytes == 0u) {
            ring->usb_flush_deferred_coalesced_bytes = (uint32_t)used;
        } else {
            ring->usb_flush_force_on_write = true;
        }
        ring->stats.usb_flush_boundary_overflows += 1u;
        return;
    }

    ring->usb_flush_pending_bytes[pending_count] = (uint32_t)used;
    ring->usb_flush_pending_count = pending_count + 1u;
}

bool bridge_ring_consume_reached_usb_flush_boundary(bridge_ring_t *ring, size_t count) {
    uint32_t coalesced_bytes;
    uint32_t deferred_coalesced_bytes;
    uint32_t pending_count;
    uint32_t first_pending = 0u;
    uint32_t shift_index;
    bool reached_boundary = false;

    if (ring == NULL) {
        return false;
    }

    pending_count = ring->usb_flush_pending_count;
    if (pending_count != 0u) {
        for (shift_index = 0u; shift_index < pending_count; ++shift_index) {
            uint32_t pending_bytes = ring->usb_flush_pending_bytes[shift_index];

            if (pending_bytes <= (uint32_t)count) {
                ring->usb_flush_pending_bytes[shift_index] = 0u;
                reached_boundary = true;
            } else {
                ring->usb_flush_pending_bytes[shift_index] = pending_bytes - (uint32_t)count;
            }
        }

        while ((first_pending < pending_count) && (ring->usb_flush_pending_bytes[first_pending] == 0u)) {
            first_pending += 1u;
        }

        if (first_pending == pending_count) {
            ring->usb_flush_pending_count = 0u;
        } else if (first_pending != 0u) {
            for (shift_index = 0u; shift_index < (pending_count - first_pending); ++shift_index) {
                ring->usb_flush_pending_bytes[shift_index] = ring->usb_flush_pending_bytes[shift_index + first_pending];
            }
            for (; shift_index < pending_count; ++shift_index) {
                ring->usb_flush_pending_bytes[shift_index] = 0u;
            }
            ring->usb_flush_pending_count = pending_count - first_pending;
        }
    }

    coalesced_bytes = ring->usb_flush_coalesced_bytes;
    deferred_coalesced_bytes = ring->usb_flush_deferred_coalesced_bytes;

    if (coalesced_bytes != 0u) {
        if (coalesced_bytes <= (uint32_t)count) {
            ring->usb_flush_coalesced_bytes = 0u;
            reached_boundary = true;
        } else {
            ring->usb_flush_coalesced_bytes = coalesced_bytes - (uint32_t)count;
        }
    }

    if (deferred_coalesced_bytes != 0u) {
        if (deferred_coalesced_bytes <= (uint32_t)count) {
            ring->usb_flush_deferred_coalesced_bytes = 0u;
            reached_boundary = true;
        } else {
            ring->usb_flush_deferred_coalesced_bytes = deferred_coalesced_bytes - (uint32_t)count;
        }
    }

    if ((ring->usb_flush_coalesced_bytes == 0u) && (ring->usb_flush_deferred_coalesced_bytes != 0u)) {
        ring->usb_flush_coalesced_bytes = ring->usb_flush_deferred_coalesced_bytes;
        ring->usb_flush_deferred_coalesced_bytes = 0u;
    }

    if (ring->usb_flush_force_on_write && (count != 0u)) {
        if ((ring->usb_flush_pending_count == 0u)
            && (ring->usb_flush_coalesced_bytes == 0u)
            && (ring->usb_flush_deferred_coalesced_bytes == 0u)
            && (bridge_ring_used(ring) == 0u)) {
            ring->usb_flush_force_on_write = false;
        }
        reached_boundary = true;
    }

    return reached_boundary;
}