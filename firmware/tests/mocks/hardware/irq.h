#ifndef MOCK_HARDWARE_IRQ_H
#define MOCK_HARDWARE_IRQ_H

#include <stdbool.h>

#define DMA_IRQ_0 0

static inline void irq_set_exclusive_handler(int irq, void (*handler)(void)) {
    (void)irq;
    (void)handler;
}

static inline void irq_set_enabled(int irq, bool enabled) {
    (void)irq;
    (void)enabled;
}

#endif