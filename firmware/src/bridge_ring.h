#ifndef BRIDGE_RING_H
#define BRIDGE_RING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bridge_config.h"

typedef struct {
    volatile uint32_t usb_write_calls;
    volatile uint32_t usb_bytes_written;
    volatile uint32_t usb_flush_calls;
    volatile uint32_t overflow_commits;
    volatile uint32_t dma_rearm_count;
    volatile uint32_t publish_invariant_failures;
    volatile uint32_t high_water_mark;
} bridge_runtime_stats_t;

/*
 * Single-producer/single-consumer ring for the current firmware data path.
 * Supported use is one serialized producer on the capture core and one
 * consumer in the foreground loop on the same core. The producer normally
 * runs in the capture IRQ path; foreground tail flushes may borrow producer
 * ownership briefly with interrupts masked. This is not a multicore-safe
 * queue.
 */
typedef struct {
    volatile uint32_t read_index;
    volatile uint32_t write_index;
    volatile uint32_t dropped_bytes;
    volatile uint32_t usb_flush_boundary_count;
    volatile bool usb_flush_boundary_reached;
    bridge_runtime_stats_t stats;
    uint32_t usb_flush_boundary_bits[BRIDGE_RING_SIZE / 32u];
    uint8_t storage[BRIDGE_RING_SIZE];
} bridge_ring_t;

void bridge_ring_init(bridge_ring_t *ring);
size_t bridge_ring_write(bridge_ring_t *ring, const uint8_t *source, size_t count);
size_t bridge_ring_read(bridge_ring_t *ring, uint8_t *destination, size_t count);
size_t bridge_ring_peek_contiguous(const bridge_ring_t *ring, const uint8_t **source);
bool bridge_ring_publish(bridge_ring_t *ring, size_t count);
void bridge_ring_produce(bridge_ring_t *ring, size_t count);
void bridge_ring_consume(bridge_ring_t *ring, size_t count);
void bridge_ring_note_usb_flush_boundary(bridge_ring_t *ring);
bool bridge_ring_consume_reached_usb_flush_boundary(bridge_ring_t *ring);

#endif