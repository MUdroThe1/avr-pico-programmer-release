#include <stdio.h>
#include <pico/stdlib.h>
#include "tusb.h"
#include "avrprog.h"
#include "stk500v1.h"

int main() {
    stdio_init_all();
    tusb_init();
    
    // Initialize AVR SPI with your wiring (adjust pins as needed)
    // If your avrprog.c provides a parameterless init, keep it; otherwise provide pins
    avr_spi_init();
    stk500v1_init();

    while (true) {
        tud_task();
        if (tud_cdc_connected() && tud_cdc_available()) {
            uint8_t rx[128];
            uint32_t n = tud_cdc_read(rx, sizeof(rx));
            if (n) stk500v1_feed(rx, (int)n);
        }
        sleep_ms(1);
    }
}
