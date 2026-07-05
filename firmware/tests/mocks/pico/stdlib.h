#ifndef MOCK_PICO_STDLIB_H
#define MOCK_PICO_STDLIB_H

#include <stdbool.h>

#define GPIO_IN 0

extern bool mock_gpio_values[32];

static inline bool gpio_get(unsigned int pin) {
    return mock_gpio_values[pin];
}

static inline void gpio_set_dir(unsigned int pin, int direction) {
    (void)pin;
    (void)direction;
}

static inline void gpio_disable_pulls(unsigned int pin) {
    (void)pin;
}

#endif