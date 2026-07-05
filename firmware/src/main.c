#include "bsp/board.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include "bridge_ring.h"
#include "spi_capture.h"
#include "spi_mosi_sniffer.pio.h"
#include "usb_stream.h"

static bridge_ring_t bridge_ring;

static void bridge_init_system_clock(void) {
    hard_assert(set_sys_clock_khz(160000u, true));
}

static void bridge_init_status_led(void) {
#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
#endif
}

static void bridge_update_status_led(void) {
#ifdef PICO_DEFAULT_LED_PIN
    gpio_put(PICO_DEFAULT_LED_PIN, !gpio_get(PICO_SPI_BRIDGE_CS_PIN));
#endif
}

int main(void) {
    bridge_ring_init(&bridge_ring);

    bridge_init_system_clock();

    spi_capture_init(&(spi_capture_config_t){
        .ring = &bridge_ring,
    });

    board_init();
    bridge_init_status_led();
    tusb_init();

    while (true) {
        tud_task();
        spi_capture_poll();
        usb_stream_poll(&bridge_ring);
        bridge_update_status_led();
        tight_loop_contents();
    }
}