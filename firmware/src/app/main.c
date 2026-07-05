#include "bsp/board.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include "bridge_ring.h"
#include "spi_capture.h"
#include "usb_stream.h"

static bridge_ring_t bridge_ring;

static void bridge_init_system_clock(void) {
    hard_assert(set_sys_clock_khz(160000u, true));
}

int main(void) {
    bridge_ring_init(&bridge_ring);

    bridge_init_system_clock();
    board_init();
    tusb_init();

    spi_capture_init(&(spi_capture_config_t){
        .ring = &bridge_ring,
    });

    while (true) {
        tud_task();
        spi_capture_poll();
        usb_stream_poll(&bridge_ring);
        tight_loop_contents();
    }
}