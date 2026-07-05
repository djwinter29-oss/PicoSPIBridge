#ifndef CAPTURE_DMA_PLAN_H
#define CAPTURE_DMA_PLAN_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t transfer_count;
    uint32_t next_reserved_write_index;
    bool drop_on_commit;
} capture_dma_plan_t;

capture_dma_plan_t capture_dma_plan_target(uint32_t reserved_write_index, uint32_t read_index);
bool capture_overflow_note_commit(bool *overflow_counted);
bool capture_poll_ready_bytes(uint32_t transfer_count, uint32_t flushed_count, uint32_t remaining_count, uint32_t *ready_count);
uint32_t capture_completion_ready_bytes(uint32_t transfer_count, uint32_t flushed_count);
void capture_note_publish_failure(uint32_t *reserved_write_index, uint32_t write_index);

#endif