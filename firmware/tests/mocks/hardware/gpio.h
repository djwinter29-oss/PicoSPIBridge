#ifndef MOCK_HARDWARE_GPIO_H
#define MOCK_HARDWARE_GPIO_H

#include <stdbool.h>
#include <stdint.h>

typedef unsigned int uint;
typedef void (*gpio_irq_callback_t)(uint gpio, uint32_t events);

extern gpio_irq_callback_t mock_gpio_irq_callback;

#define GPIO_IRQ_EDGE_RISE 0x08u

static inline void gpio_set_irq_enabled_with_callback(uint gpio, uint32_t event_mask, bool enabled, gpio_irq_callback_t callback) {
    (void)gpio;
    (void)event_mask;
    if (enabled) {
        mock_gpio_irq_callback = callback;
        return;
    }

    if (mock_gpio_irq_callback == callback) {
        mock_gpio_irq_callback = 0;
    }
}

static inline void mock_gpio_trigger_irq(uint gpio, uint32_t events) {
    if (mock_gpio_irq_callback != 0) {
        mock_gpio_irq_callback(gpio, events);
    }
}

#endif