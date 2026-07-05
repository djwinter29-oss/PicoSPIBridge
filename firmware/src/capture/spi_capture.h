#ifndef SPI_CAPTURE_H
#define SPI_CAPTURE_H

#include "bridge_ring.h"

typedef struct {
    bridge_ring_t *ring;
} spi_capture_config_t;

void spi_capture_init(const spi_capture_config_t *config);
void spi_capture_poll(void);

#endif