#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/spi.h>
#include "tusb.h"
#include "avrprog.h"
#include "avr_devices.h"
#include "stk500v1.h"
#include <stdlib.h>
#include <string.h>

uint8_t program_code[2000] = {};
char input[10] = {0};

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
    }
}
