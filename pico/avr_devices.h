/**
 * @file avr_devices.h
 * @brief AVR Device Database Types and Function Prototypes
 * 
 * This header defines the data structures and lookup functions for
 * the AVR device database. The database maps device signatures to
 * device-specific parameters needed for correct programming.
 * 
 * Device Signature Format:
 *   - Byte 0: Manufacturer (0x1E = Atmel/Microchip)
 *   - Byte 1: Flash size indicator
 *   - Byte 2: Specific device variant
 * 
 * @author MUdroThe1
 * @date 2026
 */

#pragma once

#include <stdint.h>

/**
 * @brief AVR device information structure
 * 
 * Contains all device-specific parameters needed for programming.
 * The signature uniquely identifies each AVR chip model.
 */
typedef struct {
    uint8_t signature[3];       /**< 3-byte device signature (from ISP cmd 0x30) */
    const char *name;           /**< Human-readable device name (e.g., "ATmega328P") */
    uint32_t flash_size_bytes;  /**< Total flash memory size in bytes */
    uint16_t page_size_bytes;   /**< Flash page size in bytes (for paged programming) */
} avr_device_t;

/**
 * @brief Look up device parameters by signature
 * 
 * Searches the internal device database for a matching signature
 * and returns the device profile if found.
 * 
 * @param sig 3-byte device signature to look up
 * @return Pointer to device info structure, or NULL if not found
 * 
 * @note If the device is not found, the programmer will use default
 *       page sizes which may not work correctly for the target.
 */
const avr_device_t* avr_lookup_device_by_signature(const uint8_t sig[3]);
