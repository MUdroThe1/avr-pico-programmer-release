/**
 * @file avrprog_bitbang.c
 * @brief Software SPI (Bit-banging) Implementation for AVR ISP Programming
 * 
 * This file implements a software-based SPI interface using GPIO bit-banging.
 * It provides an alternative to hardware SPI when:
 *   - Different GPIO pins are needed (hardware SPI is pin-constrained)
 *   - Hardware SPI peripheral is in use by another function
 *   - More control over timing is required for debugging
 * 
 * The implementation follows AVR ISP requirements:
 *   - SPI Mode 0: CPOL=0 (clock idles low), CPHA=0 (sample on rising edge)
 *   - MSB first data order
 *   - 4-byte transaction format
 * 
 * Performance Notes:
 *   - Default speed is ~50kHz (10us half-period delay)
 *   - Speed can be adjusted at runtime via avr_bitbang_set_speed()
 *   - Bit-banging is CPU-intensive but sufficient for ISP operations
 * 
 * @author MUdroThe1
 * @date 2026
 */

#include "avrprog_bitbang.h"
#include <stdio.h>

/*******************************************************************************
 * Private Variables
 ******************************************************************************/

/**
 * @brief Current SPI clock half-period delay in microseconds
 * 
 * This can be modified at runtime using avr_bitbang_set_speed().
 */
static uint32_t bb_delay_us = BB_DELAY_US;

/*******************************************************************************
 * Private Helper Functions
 ******************************************************************************/

/**
 * @brief Inline delay for SPI timing
 * 
 * Uses the configured delay value for clock half-period timing.
 */
static inline void bb_delay(void) {
    sleep_us(bb_delay_us);
}

/*******************************************************************************
 * Initialization Functions
 ******************************************************************************/

/**
 * @brief Initialize GPIO pins for bit-banged SPI
 * 
 * Pin Configuration:
 *   - MOSI: Output, initial low
 *   - MISO: Input with pull-up (target drives this)
 *   - SCK:  Output, initial low (SPI Mode 0 idle state)
 *   - RESET: Output, initial high (target not in reset)
 */
void avr_bitbang_init(void) {
    /* Configure MOSI as output, initially low */
    gpio_init(BB_MOSI_PIN);
    gpio_set_dir(BB_MOSI_PIN, GPIO_OUT);
    gpio_put(BB_MOSI_PIN, 0);
    
    /* Configure MISO as input with pull-up */
    gpio_init(BB_MISO_PIN);
    gpio_set_dir(BB_MISO_PIN, GPIO_IN);
    gpio_pull_up(BB_MISO_PIN);
    
    /* Configure SCK as output, initially low (Mode 0: clock idles low) */
    gpio_init(BB_SCK_PIN);
    gpio_set_dir(BB_SCK_PIN, GPIO_OUT);
    gpio_put(BB_SCK_PIN, 0);
    
    /* Configure RESET as output, initially high (target running) */
    gpio_init(BB_RESET_PIN);
    gpio_set_dir(BB_RESET_PIN, GPIO_OUT);
    gpio_put(BB_RESET_PIN, 1);
    
    /* Reset delay value to default */
    bb_delay_us = BB_DELAY_US;
}

/*******************************************************************************
 * SPI Transfer Functions
 ******************************************************************************/

/**
 * @brief Transfer a single byte via bit-banged SPI (Mode 0)
 * 
 * SPI Mode 0 Timing:
 *   1. Set MOSI to current bit value (MSB first)
 *   2. Wait half period (setup time)
 *   3. Rising edge: SCK goes high, sample MISO
 *   4. Wait half period (hold time)
 *   5. Falling edge: SCK goes low
 *   6. Repeat for all 8 bits
 * 
 * The clock signal for one bit looks like:
 * 
 *         ____
 *   SCK _|    |____
 *       ^         ^
 *       |         |
 *   Sample      Next bit
 * 
 * @param tx_byte Byte to transmit to target
 * @return Byte received from target
 */
uint8_t avr_bitbang_transfer_byte(uint8_t tx_byte) {
    uint8_t rx_byte = 0;
    
    for (int bit = 7; bit >= 0; bit--) {
        /* Set MOSI to current transmit bit (MSB first) */
        gpio_put(BB_MOSI_PIN, (tx_byte >> bit) & 0x01);
        
        /* Setup time before rising edge */
        bb_delay();
        
        /* Rising edge of SCK - target will sample MOSI, we sample MISO */
        gpio_put(BB_SCK_PIN, 1);
        
        /* Sample MISO and store in receive byte */
        if (gpio_get(BB_MISO_PIN)) {
            rx_byte |= (1 << bit);
        }
        
        /* Hold time after rising edge */
        bb_delay();
        
        /* Falling edge of SCK */
        gpio_put(BB_SCK_PIN, 0);
    }
    
    return rx_byte;
}

/**
 * @brief Transfer multiple bytes via bit-banged SPI
 * 
 * Performs a full-duplex transfer of len bytes. Each transmitted byte
 * is replaced with the received byte if tx_buf and rx_buf are the same.
 * 
 * This function is compatible with spi_write_read_blocking() signature
 * to allow easy switching between hardware and bit-banged implementations.
 * 
 * @param tx_buf Pointer to transmit data buffer
 * @param rx_buf Pointer to receive data buffer (can equal tx_buf)
 * @param len Number of bytes to transfer
 */
void avr_bitbang_transfer(const uint8_t *tx_buf, uint8_t *rx_buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        rx_buf[i] = avr_bitbang_transfer_byte(tx_buf[i]);
    }
}

/*******************************************************************************
 * Reset Control Functions
 ******************************************************************************/

/**
 * @brief Assert RESET line to hold target in programming mode
 * 
 * Drives the RESET pin low (active-low reset).
 * The target must be held in reset during ISP programming.
 */
void avr_bitbang_reset_assert(void) {
    gpio_put(BB_RESET_PIN, 0);
}

/**
 * @brief Release RESET line to allow target to run
 * 
 * Drives the RESET pin high to exit reset state.
 * The target will begin executing its program after release.
 */
void avr_bitbang_reset_release(void) {
    gpio_put(BB_RESET_PIN, 1);
}

/**
 * @brief Pulse the RESET line to restart the target
 * 
 * Generates a complete reset pulse:
 *   1. Assert reset (drive low) for 20ms
 *   2. Release reset (drive high)
 *   3. Wait 20ms for target to stabilize
 */
void avr_bitbang_reset_pulse(void) {
    avr_bitbang_reset_assert();
    sleep_ms(20);
    avr_bitbang_reset_release();
    sleep_ms(20);
}

/*******************************************************************************
 * Speed Control Functions
 ******************************************************************************/

/**
 * @brief Set the bit-bang SPI clock delay
 * 
 * Adjusts the SPI clock frequency by changing the half-period delay.
 * Approximate frequencies:
 *   - 1us   -> ~500kHz
 *   - 5us   -> ~100kHz
 *   - 10us  -> ~50kHz (default, safe for CKDIV8)
 *   - 50us  -> ~10kHz
 *   - 100us -> ~5kHz
 * 
 * @param delay_us New half-period delay in microseconds (minimum 1)
 */
void avr_bitbang_set_speed(uint32_t delay_us) {
    if (delay_us < 1) {
        delay_us = 1;  /* Enforce minimum delay */
    }
    bb_delay_us = delay_us;
}

/**
 * @brief Get the current bit-bang SPI clock delay
 * 
 * @return Current half-period delay in microseconds
 */
uint32_t avr_bitbang_get_speed(void) {
    return bb_delay_us;
}

/*******************************************************************************
 * AVR ISP Compatible Wrapper Functions
 * 
 * These functions provide the same interface as the hardware SPI version
 * in avrprog.c, allowing easy switching between implementations.
 ******************************************************************************/

#ifdef USE_BITBANG_SPI

#include "avrprog.h"

/**
 * @brief SPI transaction output buffer (same as hardware version)
 */
static uint8_t bb_output_buffer[4] = {0, 0, 0, 0};

/**
 * @brief Erase counter for flash protection
 */
static int bb_erase_count = 0;

/**
 * @brief Initialize SPI interface (bit-bang version)
 */
void avr_spi_init(void) {
    avr_bitbang_init();
}

/**
 * @brief Read the 3-byte device signature from the AVR
 * 
 * @param signature Pointer to a 3-byte buffer to store the signature
 */
void avr_read_signature(uint8_t *signature) {
    for (int i = 0; i < 3; i++) {
        uint8_t cmd[4] = {0x30, 0x00, (uint8_t)i, 0x00};
        avr_bitbang_transfer(cmd, bb_output_buffer, 4);
        signature[i] = bb_output_buffer[3];
    }
}

/**
 * @brief Perform a hardware reset pulse on the AVR target
 */
void avr_reset(void) {
    avr_bitbang_reset_pulse();
}

/**
 * @brief Enter AVR Serial Programming mode
 * 
 * @return true if programming mode entered successfully
 */
bool avr_enter_programming_mode(void) {
    avr_bitbang_reset_release();
    sleep_ms(2);
    avr_bitbang_reset_assert();

    uint8_t cmd[4] = {0xAC, 0x53, 0x00, 0x00};
    
    for (int attempt = 0; attempt < 8; attempt++) {
        avr_bitbang_transfer(cmd, bb_output_buffer, 4);
        
        if (bb_output_buffer[2] == 0x53) {
            return true;
        }
        sleep_ms(10);
    }

    avr_bitbang_reset_release();
    sleep_ms(2);
    return false;
}

/**
 * @brief Exit AVR Serial Programming mode
 */
void avr_leave_programming_mode(void) {
    avr_bitbang_reset_release();
    sleep_ms(2);
}

/**
 * @brief Perform a Chip Erase operation
 */
void avr_erase_memory(void) {
    if (bb_erase_count > 200) {
        printf("Erase limit exceeded - halting to protect flash\n");
        while (true) sleep_ms(100);
    }

    uint8_t cmd[4] = {0xAC, 0x80, 0x00, 0x00};
    avr_bitbang_transfer(cmd, bb_output_buffer, 4);
    sleep_ms(9);
    bb_erase_count++;
}

/**
 * @brief Write a word to the temporary page buffer
 */
void avr_write_temporary_buffer(uint16_t word_address, uint8_t low_byte, uint8_t high_byte) {
    uint8_t addr_msb = word_address >> 8;
    uint8_t addr_lsb = word_address & 0xFF;

    uint8_t cmd[4] = {0x40, addr_msb, addr_lsb, low_byte};
    avr_bitbang_transfer(cmd, bb_output_buffer, 4);

    uint8_t cmd2[4] = {0x48, addr_msb, addr_lsb, high_byte};
    avr_bitbang_transfer(cmd2, bb_output_buffer, 4);
}

/**
 * @brief Write a 16-bit word to the temporary page buffer
 */
void avr_write_temporary_buffer_16(uint16_t word_address, uint16_t word) {
    avr_write_temporary_buffer(word_address, word & 0xFF, word >> 8);
}

/**
 * @brief Flash the temporary page buffer to program memory
 */
void avr_flash_program_memory(uint16_t word_address) {
    uint8_t addr_msb = word_address >> 8;
    uint8_t addr_lsb = word_address & 0xFF;

    uint8_t cmd[4] = {0x4C, addr_msb, addr_lsb, 0x00};
    avr_bitbang_transfer(cmd, bb_output_buffer, 4);
    sleep_ms(5);
}

/**
 * @brief Read the low byte of a program memory word
 */
uint8_t avr_read_program_memory_low_byte(uint16_t word_address) {
    uint8_t addr_msb = word_address >> 8;
    uint8_t addr_lsb = word_address & 0xFF;

    uint8_t cmd[4] = {0x20, addr_msb, addr_lsb, 0x00};
    avr_bitbang_transfer(cmd, bb_output_buffer, 4);
    return bb_output_buffer[3];
}

/**
 * @brief Read the high byte of a program memory word
 */
uint8_t avr_read_program_memory_high_byte(uint16_t word_address) {
    uint8_t addr_msb = word_address >> 8;
    uint8_t addr_lsb = word_address & 0xFF;

    uint8_t cmd[4] = {0x28, addr_msb, addr_lsb, 0x00};
    avr_bitbang_transfer(cmd, bb_output_buffer, 4);
    return bb_output_buffer[3];
}

/**
 * @brief Read a complete 16-bit program word
 */
uint16_t avr_read_program_memory(uint16_t word_address) {
    uint16_t data = avr_read_program_memory_high_byte(word_address);
    data <<= 8;
    data |= avr_read_program_memory_low_byte(word_address);
    return data;
}

/**
 * @brief Fill page buffer from array
 */
void avr_write_temporary_buffer_page(uint16_t* data, size_t data_len) {
    if (data_len == 0) return;
    for (size_t i = 0; i < data_len; i++) {
        avr_write_temporary_buffer_16((uint16_t)i, data[i]);
    }
}

/**
 * @brief Verify programmed page against expected data
 */
bool avr_verify_program_memory_page(uint16_t page_address_start, uint16_t* expected_data, size_t data_len) {
    for (size_t i = 0; i < data_len; i++) {
        uint16_t word = avr_read_program_memory(page_address_start + i);
        if (word != expected_data[i]) {
            return false;
        }
    }
    return true;
}

#endif /* USE_BITBANG_SPI */
