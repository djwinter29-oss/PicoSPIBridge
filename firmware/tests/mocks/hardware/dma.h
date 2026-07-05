#ifndef MOCK_HARDWARE_DMA_H
#define MOCK_HARDWARE_DMA_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t transfer_count;
} mock_dma_channel_hw_t;

typedef struct {
    uint32_t ints0;
    mock_dma_channel_hw_t ch[16];
} mock_dma_hw_t;

typedef struct {
    int unused;
} dma_channel_config;

extern mock_dma_hw_t *dma_hw;
extern unsigned int mock_dma_next_channel;
extern volatile void *mock_dma_write_addresses[16];
extern uint32_t mock_dma_configure_transfer_counts[16];
extern uint32_t mock_dma_abort_calls[16];
extern uint32_t mock_dma_last_abort_sequence[16];
extern uint32_t mock_call_sequence;

#define DMA_SIZE_8 0u

static inline dma_channel_config dma_channel_get_default_config(unsigned int channel) {
    (void)channel;
    return (dma_channel_config){0};
}

static inline void channel_config_set_transfer_data_size(dma_channel_config *config, unsigned int size) {
    (void)config;
    (void)size;
}

static inline void channel_config_set_read_increment(dma_channel_config *config, bool enabled) {
    (void)config;
    (void)enabled;
}

static inline void channel_config_set_write_increment(dma_channel_config *config, bool enabled) {
    (void)config;
    (void)enabled;
}

static inline void channel_config_set_dreq(dma_channel_config *config, unsigned int dreq) {
    (void)config;
    (void)dreq;
}

static inline void channel_config_set_chain_to(dma_channel_config *config, unsigned int channel) {
    (void)config;
    (void)channel;
}

static inline void dma_channel_configure(unsigned int channel, const dma_channel_config *config, volatile void *write_addr, const volatile void *read_addr, uint32_t transfer_count, bool trigger) {
    (void)channel;
    (void)config;
    mock_dma_write_addresses[channel] = write_addr;
    (void)read_addr;
    mock_dma_configure_transfer_counts[channel] = transfer_count;
    dma_hw->ch[channel].transfer_count = transfer_count;
    (void)trigger;
}

static inline unsigned int dma_claim_unused_channel(bool required) {
    (void)required;
    return mock_dma_next_channel++;
}

static inline void dma_channel_set_irq0_enabled(unsigned int channel, bool enabled) {
    (void)channel;
    (void)enabled;
}

static inline void dma_start_channel_mask(uint32_t mask) {
    (void)mask;
}

static inline void dma_channel_abort(unsigned int channel) {
    mock_call_sequence += 1u;
    mock_dma_abort_calls[channel] += 1u;
    mock_dma_last_abort_sequence[channel] = mock_call_sequence;
    dma_hw->ch[channel].transfer_count = 0u;
}

#endif