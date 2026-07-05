#include <stddef.h>

#include "bridge_config.h"
#include "capture_dma_plan.h"

capture_dma_plan_t capture_dma_plan_target(uint32_t reserved_write_index, uint32_t read_index) {
    capture_dma_plan_t plan;
    size_t free = (size_t)((read_index - reserved_write_index - 1u) & (BRIDGE_RING_SIZE - 1u));
    size_t contiguous = BRIDGE_RING_SIZE - reserved_write_index;
    size_t transfer_count = free;

    if (transfer_count > contiguous) {
        transfer_count = contiguous;
    }

    if (transfer_count > BRIDGE_DMA_BLOCK_SIZE) {
        transfer_count = BRIDGE_DMA_BLOCK_SIZE;
    }

    if (transfer_count == 0u) {
        plan.transfer_count = BRIDGE_DMA_BLOCK_SIZE;
        plan.next_reserved_write_index = reserved_write_index;
        plan.drop_on_commit = true;
        return plan;
    }

    plan.transfer_count = (uint32_t)transfer_count;
    plan.next_reserved_write_index = (reserved_write_index + (uint32_t)transfer_count) & (BRIDGE_RING_SIZE - 1u);
    plan.drop_on_commit = false;
    return plan;
}

bool capture_overflow_note_commit(bool *overflow_counted) {
    if ((overflow_counted == NULL) || *overflow_counted) {
        return false;
    }

    *overflow_counted = true;
    return true;
}