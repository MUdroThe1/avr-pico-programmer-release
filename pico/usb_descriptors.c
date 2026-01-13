/**
 * @file usb_descriptors.c
 * @brief USB device descriptors for TinyUSB CDC implementation
 * 
 * This file defines the USB descriptors that describe the device to the USB host:
 *   - Device descriptor: Identifies the device (VID/PID, USB version, device class)
 *   - Configuration descriptor: Defines interfaces, endpoints, and power requirements
 *   - String descriptors: Human-readable manufacturer, product, and serial number strings
 * 
 * The device presents itself as a USB CDC (Communications Device Class) ACM device,
 * which creates a virtual serial port on the host system (e.g., /dev/ttyACM0 on Linux,
 * COMx on Windows). This allows the AVR programmer to communicate using standard
 * serial port APIs.
 * 
 * USB Device Configuration:
 *   - Vendor ID:  0x2E8A (Raspberry Pi)
 *   - Product ID: 0x000A (Generic CDC)
 *   - Class: CDC ACM (Abstract Control Model)
 *   - Interfaces: 2 (Control + Data)
 *   - Endpoints: 3 (Control IN, Data OUT, Data IN)
 * 
 * @author EVAbits
 * @date 2026
 */

#include <tusb.h>
#include "class/cdc/cdc_device.h"

/**
 * @brief USB device descriptor
 * 
 * The device descriptor is the first descriptor read by the host during enumeration.
 * It provides basic device information including USB version, device class, VID/PID,
 * and the number of configurations supported.
 * 
 * Key fields:
 *   - bDeviceClass: MISC class with IAD protocol for composite devices
 *   - idVendor: 0x2E8A (Raspberry Pi)
 *   - idProduct: 0x000A (Generic CDC)
 *   - bNumConfigurations: 1 (single configuration)
 */
static tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x2E8A, // Raspberry Pi (example VID)
    .idProduct          = 0x000A, // Generic CDC
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

/**
 * @brief TinyUSB callback to provide the device descriptor
 * 
 * Called by TinyUSB during USB enumeration to retrieve the device descriptor.
 * The device descriptor is the first descriptor the host reads and contains
 * fundamental device identification (VID/PID) and capabilities.
 * 
 * @return Pointer to the device descriptor structure
 */
uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const *) &desc_device;
}

/**
 * @brief Interface numbers for CDC device
 * 
 * CDC requires two interfaces:
 *   - ITF_NUM_CDC: Communication interface (control)
 *   - ITF_NUM_CDC_DATA: Data interface (bulk transfers)
 */
enum {
    ITF_NUM_CDC = 0,        ///< CDC communication interface number
    ITF_NUM_CDC_DATA,       ///< CDC data interface number
    ITF_NUM_TOTAL           ///< Total number of interfaces
};

#define EPNUM_CDC_NOTIF  0x81  ///< CDC notification endpoint (IN)
#define EPNUM_CDC_OUT    0x02  ///< CDC data OUT endpoint (host to device)
#define EPNUM_CDC_IN     0x82  ///< CDC data IN endpoint (device to host)

/**
 * @brief Full-speed configuration descriptor
 * 
 * Describes the device configuration including all interfaces and endpoints.
 * This configuration contains:
 *   - Configuration header (power requirements, interface count)
 *   - CDC Interface Association Descriptor (IAD)
 *   - CDC Communication interface with functional descriptors
 *   - CDC Data interface with bulk IN/OUT endpoints
 * 
 * The TUD_CDC_DESCRIPTOR macro generates the complete CDC descriptor hierarchy.
 * 
 * Endpoints:
 *   - 0x81: Notification IN (8 bytes) - for CDC control messages
 *   - 0x02: Data OUT (64 bytes) - receive data from host
 *   - 0x82: Data IN (64 bytes) - send data to host
 */
static uint8_t const desc_fs_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN), TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // CDC: Notification EP, Data EP OUT & IN
    TUD_CDC_DESCRIPTOR(0, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64)
};

/**
 * @brief TinyUSB callback to provide the configuration descriptor
 * 
 * Called during USB enumeration to describe the device's interfaces and endpoints.
 * This descriptor tells the host that the device has a CDC interface with
 * notification and data endpoints.
 * 
 * @param index Configuration index (unused, only one configuration supported)
 * @return Pointer to the full-speed configuration descriptor
 */
uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    return desc_fs_configuration;
}

/**
 * @brief USB string descriptors
 * 
 * Array of strings for device identification:
 *   - Index 0: Language ID (0x0409 = US English)
 *   - Index 1: Manufacturer name
 *   - Index 2: Product name
 *   - Index 3: Serial number
 *   - Index 4: CDC interface name
 */
static char const * string_desc_arr[] = {
    (const char[]){ 0x09, 0x04 }, // 0: English (0x0409)
    "EVAbits",                     // 1: Manufacturer
    "RP2040 AVR ISP",              // 2: Product
    "0001",                        // 3: Serial Number
    "CDC"                          // 4: CDC Interface
};

/** @brief Buffer for UTF-16LE encoded string descriptor (max 31 characters) */
static uint16_t _desc_str[32];

/**
 * @brief TinyUSB callback to provide string descriptors
 * 
 * Called during enumeration to retrieve human-readable strings for manufacturer,
 * product name, serial number, and interface descriptions. The host uses these
 * strings to display the device name to the user.
 * 
 * String encoding: UTF-16LE as required by USB specification
 * 
 * @param index String descriptor index (0 = language ID, 1-4 = actual strings)
 * @param langid Language ID (e.g., 0x0409 for US English)
 * @return Pointer to UTF-16LE encoded string descriptor, or NULL if invalid index
 */
uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    uint8_t chr_count;

    if (index == 0) {
        _desc_str[1] = (uint16_t) ((uint8_t const *)string_desc_arr[0])[0] | ((uint16_t)((uint8_t const *)string_desc_arr[0])[1] << 8);
        // first byte is length (in 2-byte units) and descriptor type
        _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 + 2);
        return _desc_str;
    }

    if (index >= sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) {
        return NULL;
    }

    const char* str = string_desc_arr[index];
    chr_count = (uint8_t) strlen(str);
    if (chr_count > 31) chr_count = 31;

    for (uint8_t i = 0; i < chr_count; i++) {
        _desc_str[1 + i] = str[i];
    }

    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2*chr_count + 2);
    return _desc_str;
}
