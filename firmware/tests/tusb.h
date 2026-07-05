#ifndef TUSB_H
#define TUSB_H

#include <stdbool.h>
#include <stdint.h>

bool tud_ready(void);
uint32_t tud_cdc_n_write_available(uint8_t itf);
uint32_t tud_cdc_n_write(uint8_t itf, const void *buffer, uint32_t size);
void tud_cdc_n_write_flush(uint8_t itf);
uint32_t tud_cdc_n_available(uint8_t itf);
uint32_t tud_cdc_n_read(uint8_t itf, void *buffer, uint32_t size);

#endif