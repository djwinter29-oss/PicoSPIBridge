#ifndef MOCK_HARDWARE_PIO_H
#define MOCK_HARDWARE_PIO_H

#include <stdbool.h>
#include <stdint.h>

typedef unsigned int uint;

typedef struct {
    uint32_t rxf[4];
} mock_pio_hw_t;

typedef mock_pio_hw_t *PIO;

typedef struct {
    int unused;
} pio_program_t;

extern mock_pio_hw_t mock_pio0_hw;
extern PIO pio0;

static inline uint pio_claim_unused_sm(PIO pio, bool required) {
    (void)pio;
    (void)required;
    return 0u;
}

static inline void pio_gpio_init(PIO pio, uint pin) {
    (void)pio;
    (void)pin;
}

static inline uint pio_add_program(PIO pio, const pio_program_t *program) {
    (void)pio;
    (void)program;
    return 0u;
}

static inline uint pio_get_dreq(PIO pio, uint sm, bool is_tx) {
    (void)pio;
    (void)sm;
    (void)is_tx;
    return 0u;
}

static inline bool pio_sm_is_rx_fifo_empty(PIO pio, uint sm) {
    (void)pio;
    (void)sm;
    return true;
}

#endif