#include "tusb.h"

#include "bridge_config.h"
#include "bridge_ring.h"
#include "usb_stream.h"

void usb_stream_poll(bridge_ring_t *ring) {
    uint8_t chunk[BRIDGE_USB_CHUNK_SIZE];

    if (ring == NULL) {
        return;
    }

    if (!tud_ready()) {
        return;
    }

    while (true) {
        uint32_t available = tud_cdc_n_write_available(0);
        size_t count;

        if (available == 0u) {
            break;
        }

        if (available > BRIDGE_USB_CHUNK_SIZE) {
            available = BRIDGE_USB_CHUNK_SIZE;
        }

        count = bridge_ring_read(ring, chunk, (size_t)available);
        if (count == 0u) {
            break;
        }

        (void)tud_cdc_n_write(0, chunk, count);
    }

    tud_cdc_n_write_flush(0);
}

void tud_cdc_rx_cb(uint8_t itf) {
    uint8_t sink[BRIDGE_USB_CHUNK_SIZE];

    while (tud_cdc_n_available(itf) != 0u) {
        (void)tud_cdc_n_read(itf, sink, sizeof(sink));
    }
}