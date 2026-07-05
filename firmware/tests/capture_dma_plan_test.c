#include <assert.h>
#include <stdint.h>

#include "bridge_config.h"
#include "capture_dma_plan.h"

static void test_full_block_reservation(void) {
    capture_dma_plan_t plan = capture_dma_plan_target(0u, 8192u);

    assert(plan.drop_on_commit == false);
    assert(plan.transfer_count == BRIDGE_DMA_BLOCK_SIZE);
    assert(plan.next_reserved_write_index == BRIDGE_DMA_BLOCK_SIZE);
}

static void test_wrap_limited_reservation(void) {
    uint32_t reserved_write_index = BRIDGE_RING_SIZE - 100u;
    capture_dma_plan_t plan = capture_dma_plan_target(reserved_write_index, BRIDGE_RING_SIZE / 2u);

    assert(plan.drop_on_commit == false);
    assert(plan.transfer_count == 100u);
    assert(plan.next_reserved_write_index == 0u);
}

static void test_small_free_reservation(void) {
    capture_dma_plan_t plan = capture_dma_plan_target(32u, 96u);

    assert(plan.drop_on_commit == false);
    assert(plan.transfer_count == 63u);
    assert(plan.next_reserved_write_index == 95u);
}

static void test_overflow_plan(void) {
    capture_dma_plan_t plan = capture_dma_plan_target(10u, 11u);

    assert(plan.drop_on_commit == true);
    assert(plan.transfer_count == BRIDGE_DMA_BLOCK_SIZE);
    assert(plan.next_reserved_write_index == 10u);
}

static void test_overflow_commit_is_counted_once(void) {
    bool overflow_counted = false;

    assert(capture_overflow_note_commit(&overflow_counted) == true);
    assert(overflow_counted == true);
    assert(capture_overflow_note_commit(&overflow_counted) == false);
}

static void test_overflow_commit_rejects_null_state(void) {
    assert(capture_overflow_note_commit(NULL) == false);
}

int main(void) {
    test_full_block_reservation();
    test_wrap_limited_reservation();
    test_small_free_reservation();
    test_overflow_plan();
    test_overflow_commit_is_counted_once();
    test_overflow_commit_rejects_null_state();
    return 0;
}