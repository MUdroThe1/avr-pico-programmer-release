/**
 * @file main.c
 * @brief Main entry point for RP2040 AVR ISP Programmer
 * 
 * This file contains the main application loop for the AVR ISP programmer
 * firmware. The firmware turns a Raspberry Pi Pico (RP2040) into an AVR
 * programmer that speaks the STK500v1 protocol over USB CDC.
 * 
 * Application Architecture:
 *   1. Initialize stdio, TinyUSB, and AVR SPI interface
 *   2. Enter main loop:
 *      - Process USB tasks (TinyUSB device task)
 *      - When CDC data is available, feed it to STK500v1 protocol handler
 *      - STK500v1 handler parses commands and invokes AVR programming functions
 * 
 * The USB CDC interface appears as a virtual serial port (e.g., /dev/ttyACM0)
 * which avrdude can use with the "-c arduino" programmer type.
 * 
 * @author MUdroThe1
 * @date 2026
 */

#include <stdio.h>
#include <pico/stdlib.h>
#include "tusb.h"
#include "avrprog.h"
#include "stk500v1.h"

/**
 * @brief Main application entry point
 * 
 * Initializes all subsystems and enters the main event loop.
 * The loop continuously:
 *   1. Runs TinyUSB device tasks to handle USB events
 *   2. Checks for incoming CDC data when connected
 *   3. Passes received data to STK500v1 protocol handler
 * 
 * @return Never returns (infinite loop)
 */
int main() {
    /* Initialize Pico SDK standard I/O (for debug printf if enabled) */
    stdio_init_all();
    
    /* Initialize TinyUSB stack for USB CDC functionality */
    tusb_init();
    
    /* Initialize SPI interface for AVR ISP communication */
    avr_spi_init();
    
    /* Initialize STK500v1 protocol state machine */
    stk500v1_init();

    /* Main event loop - runs forever */
    while (true) {
        /* Process pending USB events (enumeration, transfers, etc.) */
        tud_task();
        
        /* Check if USB CDC is connected and has data available */
        if (tud_cdc_connected() && tud_cdc_available()) {
            uint8_t rx[128];  /* Receive buffer for incoming CDC data */
            
            /* Read available data from USB CDC endpoint */
            uint32_t n = tud_cdc_read(rx, sizeof(rx));
            
            /* Feed received bytes to STK500v1 protocol handler */
            if (n) stk500v1_feed(rx, (int)n);
        }
        
        /* Small delay to prevent busy-looping */
        sleep_ms(1);
    }
}
