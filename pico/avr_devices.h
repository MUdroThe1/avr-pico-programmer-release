#pragma once

#include <stdint.h>

typedef struct {
    uint8_t signature[3];
    const char *name;
    uint32_t flash_size_bytes;
    uint16_t page_size_bytes; // bytes per flash page
} avr_device_t;

// Lookup device profile by 3-byte signature. Returns NULL if unknown.
const avr_device_t* avr_lookup_device_by_signature(const uint8_t sig[3]);
