/**
 * @file avr_devices.c
 * @brief AVR Device Database and Signature Lookup
 * 
 * This file contains a database of supported AVR microcontrollers and their
 * key programming parameters. The device signature (3 bytes) uniquely
 * identifies each AVR chip and is used to auto-detect the target.
 * 
 * Device Information Stored:
 *   - 3-byte signature (read via ISP command 0x30)
 *   - Device name (for debugging/display)
 *   - Flash size in bytes
 *   - Page size in bytes (critical for correct programming)
 * 
 * To add support for a new device:
 *   1. Look up the device signature in the datasheet
 *   2. Find the flash size and page size from the datasheet
 *   3. Add a new entry to the devices[] array
 * 
 * Common signature prefixes:
 *   - 0x1E: Atmel/Microchip AVR devices
 *   - Second byte: Flash size indicator
 *   - Third byte: Specific device variant
 * 
 * @author MUdroThe1
 * @date 2026
 */

#include "avr_devices.h"
#include <stddef.h>

/**
 * @brief Database of supported AVR devices
 * 
 * Each entry contains the device signature, name, flash size, and page size.
 * The page size is essential for correct page-based flash programming.
 * 
 * Add new devices here as needed for your projects.
 */
static const avr_device_t devices[] = {
    /*---------------------------------------------------------------------------
     * Device Signature Database
     * Format: { {sig[0], sig[1], sig[2]}, "Name", flash_bytes, page_bytes }
     *---------------------------------------------------------------------------*/
    
    /* ATmega328P - Popular Arduino Uno/Nano chip
     * 32KB flash, 128-byte pages (64 words per page) */
    { {0x1E, 0x95, 0x0F}, "ATmega328P", 32768, 128 },
    
    /* ATtiny85 - Popular small 8-pin AVR for projects
     * 8KB flash, 64-byte pages (32 words per page) */
    { {0x1E, 0x93, 0x0B}, "ATtiny85", 8192, 64 },
    
    /* TODO: Add more devices as needed, for example:
     * { {0x1E, 0x93, 0x07}, "ATmega8", 8192, 64 },
     * { {0x1E, 0x94, 0x03}, "ATmega168", 16384, 128 },
     * { {0x1E, 0x95, 0x14}, "ATmega328", 32768, 128 },
     * { {0x1E, 0x97, 0x05}, "ATmega1284P", 131072, 256 },
     */
};

/**
 * @brief Look up device information by signature
 * 
 * Searches the device database for a matching 3-byte signature.
 * Used to auto-detect the target device and configure page sizes.
 * 
 * @param sig 3-byte device signature (read from target via READ_SIGN)
 * @return Pointer to device info if found, NULL if unknown device
 * 
 * @note If NULL is returned, programming will use default page size
 *       which may not be correct for the actual device.
 */
const avr_device_t* avr_lookup_device_by_signature(const uint8_t sig[3]) {
    /* Linear search through device database */
    for (size_t i = 0; i < sizeof(devices) / sizeof(devices[0]); i++) {
        if (devices[i].signature[0] == sig[0] &&
            devices[i].signature[1] == sig[1] &&
            devices[i].signature[2] == sig[2]) {
            return &devices[i];
        }
    }
    return NULL;  /* Device not found in database */
}
