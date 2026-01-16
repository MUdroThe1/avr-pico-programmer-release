/**
 * @file avrprog.h
 * @brief AVR ISP Programming Interface - Function Prototypes
 * 
 * This header defines the public interface for AVR In-System Programming (ISP)
 * functions. These functions implement the low-level AVR ISP protocol over SPI.
 * 
 * SPI Implementation Options:
 *   - Hardware SPI (default): Uses RP2040's SPI0 peripheral for fast transfers
 *   - Bit-bang SPI: Software implementation using GPIO, allows any pins
 * 
 * To use bit-bang mode, build with: cmake -DUSE_BITBANG_SPI=ON ..
 * 
 * AVR ISP Protocol Overview:
 *   - All commands are 4-byte SPI transactions
 *   - SPI Mode 0 (CPOL=0, CPHA=0, MSB first)
 *   - Target held in reset during programming
 *   - Page-based flash programming model
 * 
 * Typical Programming Sequence:
 *   1. avr_spi_init() - Initialize SPI interface
 *   2. avr_enter_programming_mode() - Put target in programming mode
 *   3. avr_read_signature() - Verify correct target connected
 *   4. avr_erase_memory() - Erase flash before programming
 *   5. Loop: avr_write_temporary_buffer*() + avr_flash_program_memory()
 *   6. Verify with avr_verify_program_memory_page()
 *   7. avr_leave_programming_mode() - Release target to run
 * 
 * @author MUdroThe1
 * @date 2026
 */

#pragma once

#include <pico/stdlib.h>

/* Include bit-bang header for access to speed control functions */
#ifdef USE_BITBANG_SPI
#include "avrprog_bitbang.h"
#endif

/*******************************************************************************
 * Initialization and Mode Control Functions
 ******************************************************************************/

/**
 * @brief Initialize SPI interface for AVR ISP communication
 * 
 * Configures GPIO pins and SPI peripheral for communication with AVR target.
 * Must be called before any other AVR programming functions.
 */
void avr_spi_init();

/**
 * @brief Pulse RESET line to restart target
 * 
 * Generates an active-low reset pulse to restart the AVR target.
 * Useful for recovery from stuck states.
 */
void avr_reset();

/**
 * @brief Enter Serial Programming mode
 * 
 * Holds RESET low and sends Programming Enable command to put the
 * target into ISP mode. Retries automatically if synchronization fails.
 * 
 * @return true if programming mode entered successfully, false if failed
 */
bool avr_enter_programming_mode();

/**
 * @brief Exit Serial Programming mode
 * 
 * Releases RESET line to allow target to exit programming mode and run.
 */
void avr_leave_programming_mode();

/*******************************************************************************
 * Memory Operations
 ******************************************************************************/

/**
 * @brief Perform Chip Erase operation
 * 
 * Erases entire flash and EEPROM. Required before programming new data.
 * 
 * @warning This operation wears flash - limited to ~10,000 cycles total.
 * @note A safety limit prevents more than 200 erases per session.
 */
void avr_erase_memory();

/**
 * @brief Read 3-byte device signature
 * 
 * @param signature Buffer to store 3-byte signature (must be >= 3 bytes)
 */
void avr_read_signature(uint8_t *signature);

/*******************************************************************************
 * Page Buffer Write Functions
 ******************************************************************************/

/**
 * @brief Write a 16-bit word to page buffer
 * 
 * @param word_address Word offset within page (0 = first word)
 * @param data 16-bit program word to write
 */
void avr_write_temporary_buffer_16(uint16_t word_address, uint16_t data);

/**
 * @brief Write low and high bytes separately to page buffer
 * 
 * @param word_address Word offset within page
 * @param low_byte Lower 8 bits of program word
 * @param high_byte Upper 8 bits of program word
 */
void avr_write_temporary_buffer(uint16_t word_address, uint8_t low_byte, uint8_t high_byte);

/**
 * @brief Fill page buffer from array
 * 
 * @param data Pointer to word array
 * @param data_len Number of words to write (must not exceed device page size)
 */
void avr_write_temporary_buffer_page(uint16_t* data, size_t data_len);

/*******************************************************************************
 * Flash Programming Functions
 ******************************************************************************/

/**
 * @brief Commit page buffer to flash memory
 * 
 * Writes the entire page buffer contents to flash at the specified page.
 * 
 * @param word_address Any word address within the target page
 */
void avr_flash_program_memory(uint16_t word_address);

/*******************************************************************************
 * Memory Read Functions
 ******************************************************************************/

/**
 * @brief Read low byte of program word
 * 
 * @param word_address Word address to read
 * @return Lower 8 bits of the program word
 */
uint8_t avr_read_program_memory_low_byte(uint16_t word_address);

/**
 * @brief Read high byte of program word
 * 
 * @param word_address Word address to read
 * @return Upper 8 bits of the program word
 */
uint8_t avr_read_program_memory_high_byte(uint16_t word_address);

/**
 * @brief Read complete 16-bit program word
 * 
 * @param word_address Word address to read
 * @return 16-bit program word
 */
uint16_t avr_read_program_memory(uint16_t word_address);

/*******************************************************************************
 * Verification Functions
 ******************************************************************************/

/**
 * @brief Verify programmed page against expected data
 * 
 * Reads back a page of flash and compares against expected values.
 * Essential for detecting programming errors.
 * 
 * @param page_address_start Starting word address of page
 * @param expected_data Expected data array
 * @param data_len Number of words to verify
 * @return true if verification passed, false if mismatch found
 */
bool avr_verify_program_memory_page(uint16_t page_address_start, uint16_t* expected_data, size_t data_len);
