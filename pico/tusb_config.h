#pragma once

// TinyUSB device configuration for RP2040
// Ensures CDC class is enabled so tud_cdc_* symbols are available

#include <tusb_option.h>

#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU OPT_MCU_RP2040
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_PICO
#endif

#ifndef CFG_TUSB_RHPORT0_MODE
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#endif

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))
#endif

#define CFG_TUD_ENDPOINT0_SIZE 64

// Enable only CDC to keep firmware small
#define CFG_TUD_CDC     1
#define CFG_TUD_MSC     0
#define CFG_TUD_HID     0
#define CFG_TUD_MIDI    0
#define CFG_TUD_VENDOR  0
#define CFG_TUD_NET     0
#define CFG_TUD_USBTMC  0
#define CFG_TUD_DFU     0
#define CFG_TUD_VIDEO   0

// CDC buffer sizes
#define CFG_TUD_CDC_RX_BUFSIZE 256
#define CFG_TUD_CDC_TX_BUFSIZE 256
