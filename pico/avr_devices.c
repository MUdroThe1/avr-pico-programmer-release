#include "avr_devices.h"
#include <stddef.h>

static const avr_device_t devices[] = {
    // Devices Data Base
    { {0x1E, 0x95, 0x0F}, "ATmega328P", 32768, 128 },
    { {0x1E, 0x93, 0x0B}, "ATtiny85", 8192, 64 }, // ATtiny85 - 8KB flash, 64-byte pages (32 words)
    // (Add more devices here as needed)
};

const avr_device_t* avr_lookup_device_by_signature(const uint8_t sig[3]) {
    for (size_t i = 0; i < sizeof(devices) / sizeof(devices[0]); i++) {
        if (devices[i].signature[0] == sig[0] &&
            devices[i].signature[1] == sig[1] &&
            devices[i].signature[2] == sig[2]) {
            return &devices[i];
        }
    }
    return NULL;
}
