/**
 * @file stk500v1.c
 * @brief STK500v1 Protocol Implementation for AVR ISP Programming
 * 
 * This file implements a subset of the Atmel STK500 version 1 protocol,
 * which is the protocol used by Arduino bootloaders and avrdude's
 * "-c arduino" programmer type.
 * 
 * Protocol Overview:
 *   - Commands are framed with Sync_CRC_EOP (0x20) as terminator
 *   - All responses start with Resp_STK_INSYNC (0x14)
 *   - Successful responses end with Resp_STK_OK (0x10)
 *   - Failed responses end with Resp_STK_FAILED (0x11)
 * 
 * Supported Commands:
 *   - GET_SYNC: Synchronization/ping
 *   - GET_SIGN_ON: Returns programmer identification
 *   - GET/SET_PARAMETER: Read/write programmer parameters
 *   - SET_DEVICE: Configure target device parameters
 *   - ENTER/LEAVE_PROGMODE: Enter/exit programming mode
 *   - CHIP_ERASE: Erase target flash memory
 *   - LOAD_ADDRESS: Set current address for read/write
 *   - PROG_PAGE: Write a page of flash memory
 *   - READ_PAGE: Read a page of flash memory
 *   - READ_SIGN: Read target device signature
 *   - UNIVERSAL: Raw 4-byte SPI transaction
 * 
 * Reference: Atmel AVR061 - STK500 Communication Protocol
 * 
 * @author MUdroThe1
 * @date 2026
 */

#include <string.h>
#include "pico/stdlib.h"
#include "tusb.h"
#include "stk500v1.h"
#include "avrprog.h"
#include "avr_devices.h"
#include <hardware/spi.h>

/*******************************************************************************
 * Protocol State Variables
 ******************************************************************************/

/** Current word address for page read/write operations */
static uint32_t current_address = 0;

/** Flag indicating if target is in programming mode */
static bool programming = false;

/** Page size in bytes for current target device (default 128 for ATmega328P) */
static uint16_t page_size_bytes = 128;

/** Number of 16-bit words per flash page */
static uint16_t words_per_page = 64;

/*******************************************************************************
 * Stream Buffer for STK500v1 Frame Parsing
 * 
 * STK500v1 commands are variable-length and terminated by Sync_CRC_EOP (0x20).
 * This buffer accumulates incoming bytes until a complete frame is received.
 ******************************************************************************/
#define STK_RX_BUF_SIZE 1024
static uint8_t rx_buf[STK_RX_BUF_SIZE];
static size_t rx_len = 0;

/*******************************************************************************
 * Helper Functions for USB CDC Response Transmission
 ******************************************************************************/

static inline void put(uint8_t b) { tud_cdc_write_char(b); }
static inline void flush(void) { tud_cdc_write_flush(); }

static inline void resp_ok_insync(void) { put(Resp_STK_INSYNC); put(Resp_STK_OK); flush(); }
static inline void resp_failed(void) { put(Resp_STK_INSYNC); put(Resp_STK_FAILED); flush(); }

static inline void resp_nosync(void) { put(Resp_STK_NOSYNC); flush(); }

static void drop_rx(size_t n) {
    if (n >= rx_len) {
        rx_len = 0;
        return;
    }
    memmove(rx_buf, rx_buf + n, rx_len - n);
    rx_len -= n;
}

static uint8_t get_parameter_value(uint8_t param) {
    // Values are mostly informational for avrdude; keep stable.
    switch (param) {
        case 0x80: return 0x02; // HWVER
        case 0x81: return 0x01; // SW_MAJOR
        case 0x82: return 0x12; // SW_MINOR (18)
        default: return 0x00;
    }
}

static void cache_device_params(void) {
    uint8_t sig[3] = {0};
    avr_read_signature(sig);
    const avr_device_t* dev = avr_lookup_device_by_signature(sig);
    if (dev && dev->page_size_bytes) {
        page_size_bytes = dev->page_size_bytes;
        words_per_page = page_size_bytes / 2;
    }
}

/**
 * @brief Process a complete STK500v1 command frame
 * 
 * This is the main command dispatcher for the STK500v1 protocol.
 * Each command type is handled according to the protocol specification.
 * 
 * @param cmd         Command byte (first byte of frame)
 * @param payload     Pointer to payload bytes (between cmd and EOP)
 * @param payload_len Number of payload bytes
 */
static void handle_frame(uint8_t cmd, const uint8_t* payload, size_t payload_len) {
    switch (cmd) {
        
        /*------------------------------------------------------------------
         * GET_SYNC (0x30): Synchronization ping
         * avrdude uses this to verify communication with programmer
         *------------------------------------------------------------------*/
        case Cmnd_STK_GET_SYNC: {
            resp_ok_insync();
        } break;

        /*------------------------------------------------------------------
         * GET_SIGN_ON (0x31): Return programmer identification string
         * Identifies this as an "AVR ISP" compatible programmer
         *------------------------------------------------------------------*/
        case Cmnd_STK_GET_SIGN_ON: {
            static const uint8_t sign_on[] = {'A','V','R',' ','I','S','P'};
            put(Resp_STK_INSYNC);
            tud_cdc_write(sign_on, sizeof(sign_on));
            put(Resp_STK_OK);
            flush();
        } break;

        /*------------------------------------------------------------------
         * GET_PARAMETER (0x41): Read programmer parameter
         * Returns hardware/software version info
         *------------------------------------------------------------------*/
        case Cmnd_STK_GET_PARAMETER: {
            if (payload_len != 1) {
                resp_failed();
                break;
            }
            put(Resp_STK_INSYNC);
            put(get_parameter_value(payload[0]));
            put(Resp_STK_OK);
            flush();
        } break;

        /*------------------------------------------------------------------
         * SET_PARAMETER (0x40): Write programmer parameter
         * Accepts but ignores - no configurable parameters
         *------------------------------------------------------------------*/
        case Cmnd_STK_SET_PARAMETER: {
            if (payload_len != 2) {
                resp_failed();
                break;
            }
            resp_ok_insync();
        } break;

        /*------------------------------------------------------------------
         * SET_DEVICE (0x42): Set target device parameters
         * avrdude sends 20 bytes of device info; we accept and ignore
         * (device is auto-detected from signature)
         *------------------------------------------------------------------*/
        case Cmnd_STK_SET_DEVICE: {
            resp_ok_insync();
        } break;

        /*------------------------------------------------------------------
         * SET_DEVICE_EXT (0x45): Extended device parameters
         * avrdude sends 5 bytes; accept and ignore
         *------------------------------------------------------------------*/
        case Cmnd_STK_SET_DEVICE_EXT: {
            resp_ok_insync();
        } break;

        /*------------------------------------------------------------------
         * ENTER_PROGMODE (0x50): Enter target programming mode
         * Puts AVR target into ISP mode for programming
         *------------------------------------------------------------------*/
        case Cmnd_STK_ENTER_PROGMODE: {
            if (avr_enter_programming_mode()) {
                programming = true;
                cache_device_params();  /* Auto-detect target page size */
                resp_ok_insync();
            } else {
                resp_failed();
            }
        } break;

        /*------------------------------------------------------------------
         * LEAVE_PROGMODE (0x51): Exit target programming mode
         * Releases AVR reset so target can run
         *------------------------------------------------------------------*/
        case Cmnd_STK_LEAVE_PROGMODE: {
            programming = false;
            avr_leave_programming_mode();
            resp_ok_insync();
        } break;

        /*------------------------------------------------------------------
         * CHIP_ERASE (0x52): Erase target flash memory
         * Must be done before programming new data
         *------------------------------------------------------------------*/
        case Cmnd_STK_CHIP_ERASE: {
            avr_erase_memory();
            resp_ok_insync();
        } break;

        /*------------------------------------------------------------------
         * CHECK_AUTOINC (0x53): Check if address auto-increments
         * We support auto-increment, so return 1
         *------------------------------------------------------------------*/
        case Cmnd_STK_CHECK_AUTOINC: {
            put(Resp_STK_INSYNC);
            put(1);  /* Yes, we support auto-increment */
            put(Resp_STK_OK);
            flush();
        } break;

        /*------------------------------------------------------------------
         * LOAD_ADDRESS (0x55): Set current read/write address
         * Payload: [addr_low, addr_high] (word address, not byte)
         *------------------------------------------------------------------*/
        case Cmnd_STK_LOAD_ADDRESS: {
            if (payload_len != 2) {
                resp_failed();
                break;
            }
            current_address = ((uint32_t)payload[1] << 8) | payload[0];
            resp_ok_insync();
        } break;

        /*------------------------------------------------------------------
         * READ_SIGN (0x75): Read target device signature
         * Returns 3-byte signature identifying the AVR chip
         *------------------------------------------------------------------*/
        case Cmnd_STK_READ_SIGN: {
            uint8_t sig[3] = {0};
            avr_read_signature(sig);
            put(Resp_STK_INSYNC);
            put(sig[0]); put(sig[1]); put(sig[2]);
            put(Resp_STK_OK);
            flush();
        } break;

        /*------------------------------------------------------------------
         * UNIVERSAL (0x56): Raw 4-byte SPI transaction
         * Allows avrdude to send arbitrary ISP commands (e.g., fuse read)
         * Response is the 4th byte of the SPI exchange
         *------------------------------------------------------------------*/
        case Cmnd_STK_UNIVERSAL: {
            if (payload_len != 4) {
                resp_failed();
                break;
            }
            uint8_t rx[4] = {0};
            spi_write_read_blocking(spi0, payload, rx, 4);
            put(Resp_STK_INSYNC);
            put(rx[3]);  /* Return 4th byte of SPI response */
            put(Resp_STK_OK);
            flush();
        } break;

        /*------------------------------------------------------------------
         * PROG_PAGE (0x64): Write a page of flash memory
         * Payload: [size_hi, size_lo, memtype, data...]
         *------------------------------------------------------------------*/
        case Cmnd_STK_PROG_PAGE: {
            if (payload_len < 3) {
                resp_failed();
                break;
            }
            
            int size = ((int)payload[0] << 8) | payload[1];  /* Byte count */
            uint8_t memtype = payload[2];                    /* 'F' for flash */
            const uint8_t* data = payload + 3;
            size_t data_len = payload_len - 3;

            /* Validate: only flash programming, correct size */
            if (!(memtype == 'F' || memtype == 'f') || size < 0 || (size_t)size != data_len) {
                resp_failed();
                break;
            }
            if ((size_t)size > page_size_bytes || (size_t)size > 256) {
                resp_failed();
                break;
            }
            
            /* Load data into page buffer (as words, even byte count) */
            int words = size / 2;
            for (int j = 0; j < words; j++) {
                uint16_t low = data[j * 2 + 0];
                uint16_t high = data[j * 2 + 1];
                uint16_t word = (high << 8) | low;
                avr_write_temporary_buffer_16((uint16_t)j, word);
            }
            
            /* Commit page buffer to flash at current address */
            avr_flash_program_memory((uint16_t)current_address);
            current_address += (uint32_t)words;  /* Auto-increment address */
            resp_ok_insync();
        } break;

        /*------------------------------------------------------------------
         * READ_PAGE (0x74): Read a page of flash memory
         * Payload: [size_hi, size_lo, memtype]
         *------------------------------------------------------------------*/
        case Cmnd_STK_READ_PAGE: {
            if (payload_len != 3) {
                resp_failed();
                break;
            }
            int size = ((int)payload[0] << 8) | payload[1];  /* Byte count */
            uint8_t memtype = payload[2];                    /* 'F' for flash */
            
            /* Validate: only flash reading, reasonable size */
            if (!(memtype == 'F' || memtype == 'f') || size <= 0 || size > 256) {
                resp_failed();
                break;
            }

            /* Read bytes from flash and send via CDC */
            put(Resp_STK_INSYNC);
            for (int off = 0; off < size; off++) {
                /* Read word, extract appropriate byte (low or high) */
                uint16_t w = avr_read_program_memory((uint16_t)(current_address + (uint32_t)(off / 2)));
                uint8_t b = (off & 1) ? (uint8_t)(w >> 8) : (uint8_t)(w & 0xFF);
                tud_cdc_write_char(b);
            }
            put(Resp_STK_OK);
            flush();
            current_address += (uint32_t)((size + 1) / 2);  /* Auto-increment */
        } break;

        /*------------------------------------------------------------------
         * Default: Unknown command - respond with failure
         *------------------------------------------------------------------*/
        default: {
            resp_failed();
        } break;
    }
}

/**
 * @brief Initialize STK500v1 protocol handler state
 * 
 * Resets all state variables to their default values.
 * Should be called once at startup.
 */
void stk500v1_init(void) {
    current_address = 0;
    programming = false;
    page_size_bytes = 128;            /* Default for ATmega328P */
    words_per_page = page_size_bytes / 2;
    rx_len = 0;
}

/**
 * @brief Feed received bytes into the STK500v1 protocol parser
 * 
 * This function accumulates incoming bytes in a buffer and parses
 * complete STK500v1 command frames. Each command is terminated by
 * Sync_CRC_EOP (0x20). When a complete frame is detected, it is
 * dispatched to handle_frame() for processing.
 * 
 * Frame parsing handles:
 *   - Variable-length commands based on command type
 *   - Frame synchronization and resync on errors
 *   - Buffer overflow protection
 * 
 * @param data Pointer to received bytes
 * @param len  Number of bytes received
 */
void stk500v1_feed(const uint8_t* data, int len) {
    if (!data || len <= 0) return;

    /* Append incoming data to RX buffer (truncate if overflow) */
    size_t to_copy = (size_t)len;
    if (to_copy > (STK_RX_BUF_SIZE - rx_len)) {
        to_copy = STK_RX_BUF_SIZE - rx_len;
    }
    if (to_copy > 0) {
        memcpy(rx_buf + rx_len, data, to_copy);
        rx_len += to_copy;
    }

    /* Parse and process complete frames */
    while (rx_len > 0) {
        /* Skip stray EOP bytes from previous desync */
        if (rx_buf[0] == Sync_CRC_EOP) {
            drop_rx(1);
            continue;
        }

        uint8_t cmd = rx_buf[0];
        size_t needed = 0;

        /*--------------------------------------------------------------
         * Determine expected frame length based on command type
         *--------------------------------------------------------------*/
        switch (cmd) {
            /* Commands with no payload (just cmd + EOP) */
            case Cmnd_STK_GET_SYNC:
            case Cmnd_STK_GET_SIGN_ON:
            case Cmnd_STK_ENTER_PROGMODE:
            case Cmnd_STK_LEAVE_PROGMODE:
            case Cmnd_STK_CHIP_ERASE:
            case Cmnd_STK_CHECK_AUTOINC:
            case Cmnd_STK_READ_SIGN:
                needed = 1 + 1;  /* cmd + EOP */
                break;

            case Cmnd_STK_GET_PARAMETER:
                needed = 1 + 1 + 1;  /* cmd + param + EOP */
                break;

            case Cmnd_STK_SET_PARAMETER:
                needed = 1 + 2 + 1;  /* cmd + (param, value) + EOP */
                break;

            case Cmnd_STK_SET_DEVICE:
                needed = 1 + 20 + 1;  /* cmd + 20 device bytes + EOP */
                break;

            case Cmnd_STK_SET_DEVICE_EXT:
                needed = 1 + 5 + 1;  /* cmd + 5 extended bytes + EOP */
                break;

            case Cmnd_STK_LOAD_ADDRESS:
                needed = 1 + 2 + 1;  /* cmd + (addr_lo, addr_hi) + EOP */
                break;

            case Cmnd_STK_UNIVERSAL:
                needed = 1 + 4 + 1;  /* cmd + 4 SPI bytes + EOP */
                break;

            case Cmnd_STK_READ_PAGE:
                needed = 1 + 3 + 1;  /* cmd + (size_hi, size_lo, memtype) + EOP */
                break;

            /* PROG_PAGE has variable length based on embedded size */
            case Cmnd_STK_PROG_PAGE: {
                if (rx_len < 4) {
                    /* Need header to determine total length */
                    return;
                }
                int size = ((int)rx_buf[1] << 8) | rx_buf[2];
                if (size < 0 || size > 256) {
                    /* Invalid size - resync */
                    drop_rx(1);
                    continue;
                }
                needed = 1 + 3 + (size_t)size + 1;  /* cmd + header + data + EOP */
            } break;

            default:
                /* Unknown command - drop byte and try again */
                drop_rx(1);
                continue;
        }

        /* Wait for more bytes if frame incomplete */
        if (needed == 0) {
            drop_rx(1);
            continue;
        }
        if (rx_len < needed) {
            return;
        }
        
        /* Verify EOP terminator */
        if (rx_buf[needed - 1] != Sync_CRC_EOP) {
            /* Frame error - try to resync */
            void* eop_ptr = memchr(rx_buf, Sync_CRC_EOP, rx_len);
            if (eop_ptr) {
                size_t idx = (size_t)((uint8_t*)eop_ptr - rx_buf);
                drop_rx(idx + 1);
            } else {
                drop_rx(1);
            }
            resp_nosync();
            continue;
        }

        /* Frame complete - dispatch to handler */
        const uint8_t* payload = rx_buf + 1;
        size_t payload_len = needed - 2;  /* Exclude cmd and EOP */
        (void)programming;  /* Suppress unused warning */
        handle_frame(cmd, payload, payload_len);
        drop_rx(needed);
    }
}
