#ifndef MOCK_PICO_STDLIB_H
#define MOCK_PICO_STDLIB_H

#include <stdbool.h>

#define GPIO_IN 0

static inline bool gpio_get(unsigned int pin) {
    (void)pin;
    return true;
}

static inline void gpio_set_dir(unsigned int pin, int direction) {
    (void)pin;
    (void)direction;
}

static inline void gpio_disable_pulls(unsigned int pin) {
    (void)pin;
}

#endif