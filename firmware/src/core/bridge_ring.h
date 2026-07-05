#ifndef BRIDGE_RING_H
#define BRIDGE_RING_H

#include <stddef.h>
#include <stdint.h>

#include "bridge_config.h"

/*
 * Single-producer/single-consumer ring for the current firmware data path.
 * Supported use is one producer in the capture IRQ path and one consumer in
 * the foreground loop on the same core. This is not a multicore-safe queue.
 */
typedef struct {
    volatile uint32_t read_index;
    volatile uint32_t write_index;
    volatile uint32_t dropped_bytes;
    uint8_t storage[BRIDGE_RING_SIZE];
} bridge_ring_t;

void bridge_ring_init(bridge_ring_t *ring);
size_t bridge_ring_write(bridge_ring_t *ring, const uint8_t *source, size_t count);
size_t bridge_ring_read(bridge_ring_t *ring, uint8_t *destination, size_t count);

#endif