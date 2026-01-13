/**
 * @file tusb_config.h
 * @brief TinyUSB stack configuration for RP2040 AVR ISP programmer
 * 
 * This file configures the TinyUSB USB device stack parameters including:
 *   - Target microcontroller (RP2040)
 *   - Enabled device classes (CDC only)
 *   - Buffer sizes and endpoint configurations
 *   - USB speed (Full-Speed 12 Mbps)
 *   - Memory alignment and section attributes
 * 
 * Configuration Philosophy:
 * Only CDC is enabled to minimize firmware size. Other USB classes (MSC, HID, MIDI, etc.)
 * are disabled since this programmer only requires serial communication.
 * 
 * @author EVAbits
 * @date 2026
 */

#pragma once

// TinyUSB device configuration for RP2040
// Ensures CDC class is enabled so tud_cdc_* symbols are available

#include <tusb_option.h>

#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU OPT_MCU_RP2040  ///< Target MCU: Raspberry Pi RP2040
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_PICO  ///< Operating system: Pico SDK
#endif

#ifndef CFG_TUSB_RHPORT0_MODE
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)  ///< USB device mode at Full-Speed (12 Mbps)
#endif

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION  ///< Memory section for TinyUSB (default)
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))  ///< 4-byte memory alignment for RP2040
#endif

#define CFG_TUD_ENDPOINT0_SIZE 64  ///< Control endpoint 0 size (64 bytes standard for Full-Speed)

/**
 * @name USB Device Class Configuration
 * @brief Enable/disable specific USB device classes
 * @{
 */
#define CFG_TUD_CDC     1  ///< Enable CDC (Communications Device Class) - required for serial communication
#define CFG_TUD_MSC     0  ///< Disable MSC (Mass Storage Class)
#define CFG_TUD_HID     0  ///< Disable HID (Human Interface Device)
#define CFG_TUD_MIDI    0  ///< Disable MIDI
#define CFG_TUD_VENDOR  0  ///< Disable vendor-specific class
#define CFG_TUD_NET     0  ///< Disable network class
#define CFG_TUD_USBTMC  0  ///< Disable USB Test and Measurement Class
#define CFG_TUD_DFU     0  ///< Disable DFU (Device Firmware Update)
#define CFG_TUD_VIDEO   0  ///< Disable video class
/** @} */

/**
 * @name CDC Buffer Configuration
 * @brief FIFO buffer sizes for CDC transmit and receive
 * @{
 */
#define CFG_TUD_CDC_RX_BUFSIZE 256  ///< CDC receive buffer size (256 bytes)
#define CFG_TUD_CDC_TX_BUFSIZE 256  ///< CDC transmit buffer size (256 bytes)
/** @} */
