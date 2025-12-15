#include <string.h>
#include "pico/stdlib.h"
#include "tusb.h"
#include "stk500v1.h"
#include "avrprog.h"
#include "avr_devices.h"
#include <hardware/spi.h>

// Simple state for address and mode
static uint32_t current_address = 0; // word address
static bool programming = false;
static uint16_t page_size_bytes = 128; // default, updated after signature lookup
static uint16_t words_per_page = 64;   // default for 128B pages

// Stream RX buffer (STK500v1 is framed by Sync_CRC_EOP)
#define STK_RX_BUF_SIZE 1024
static uint8_t rx_buf[STK_RX_BUF_SIZE];
static size_t rx_len = 0;

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

static void handle_frame(uint8_t cmd, const uint8_t* payload, size_t payload_len) {
    // payload excludes cmd and Sync_CRC_EOP
    switch (cmd) {
        case Cmnd_STK_GET_SYNC: {
            resp_ok_insync();
        } break;

        case Cmnd_STK_GET_SIGN_ON: {
            // STK500v1 sign-on string
            static const uint8_t sign_on[] = {'A','V','R',' ','I','S','P'};
            put(Resp_STK_INSYNC);
            tud_cdc_write(sign_on, sizeof(sign_on));
            put(Resp_STK_OK);
            flush();
        } break;

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

        case Cmnd_STK_SET_PARAMETER: {
            // payload: param, value
            if (payload_len != 2) {
                resp_failed();
                break;
            }
            resp_ok_insync();
        } break;

        case Cmnd_STK_SET_DEVICE: {
            // avrdude sends 20 bytes; accept and ignore
            resp_ok_insync();
        } break;

        case Cmnd_STK_SET_DEVICE_EXT: {
            // avrdude sends 5 bytes; accept and ignore
            resp_ok_insync();
        } break;

        case Cmnd_STK_ENTER_PROGMODE: {
            if (avr_enter_programming_mode()) {
                programming = true;
                cache_device_params();
                resp_ok_insync();
            } else {
                resp_failed();
            }
        } break;

        case Cmnd_STK_LEAVE_PROGMODE: {
            programming = false;
            resp_ok_insync();
        } break;

        case Cmnd_STK_CHIP_ERASE: {
            avr_erase_memory();
            resp_ok_insync();
        } break;

        case Cmnd_STK_CHECK_AUTOINC: {
            put(Resp_STK_INSYNC);
            put(1); // support autoincrement
            put(Resp_STK_OK);
            flush();
        } break;

        case Cmnd_STK_LOAD_ADDRESS: {
            if (payload_len != 2) {
                resp_failed();
                break;
            }
            // payload: addr_low, addr_high (word address)
            current_address = ((uint32_t)payload[1] << 8) | payload[0];
            resp_ok_insync();
        } break;

        case Cmnd_STK_READ_SIGN: {
            uint8_t sig[3] = {0};
            avr_read_signature(sig);
            put(Resp_STK_INSYNC);
            put(sig[0]); put(sig[1]); put(sig[2]);
            put(Resp_STK_OK);
            flush();
        } break;

        case Cmnd_STK_UNIVERSAL: {
            if (payload_len != 4) {
                resp_failed();
                break;
            }
            // Raw 4-byte SPI transaction; response is the 4th byte.
            uint8_t rx[4] = {0};
            spi_write_read_blocking(spi0, payload, rx, 4);
            put(Resp_STK_INSYNC);
            put(rx[3]);
            put(Resp_STK_OK);
            flush();
        } break;

        case Cmnd_STK_PROG_PAGE: {
            // payload: size_hi, size_lo, memtype, data...
            if (payload_len < 3) {
                resp_failed();
                break;
            }
            int size = ((int)payload[0] << 8) | payload[1];
            uint8_t memtype = payload[2];
            const uint8_t* data = payload + 3;
            size_t data_len = payload_len - 3;

            if (!(memtype == 'F' || memtype == 'f') || size < 0 || (size_t)size != data_len) {
                resp_failed();
                break;
            }
            if ((size_t)size > page_size_bytes || (size_t)size > 256) {
                resp_failed();
                break;
            }
            // Program as words; avrdude uses even sizes for flash.
            int words = size / 2;
            for (int j = 0; j < words; j++) {
                uint16_t low = data[j * 2 + 0];
                uint16_t high = data[j * 2 + 1];
                uint16_t word = (high << 8) | low;
                avr_write_temporary_buffer_16((uint16_t)j, word);
            }
            avr_flash_program_memory((uint16_t)current_address);
            current_address += (uint32_t)words;
            resp_ok_insync();
        } break;

        case Cmnd_STK_READ_PAGE: {
            // payload: size_hi, size_lo, memtype
            if (payload_len != 3) {
                resp_failed();
                break;
            }
            int size = ((int)payload[0] << 8) | payload[1];
            uint8_t memtype = payload[2];
            if (!(memtype == 'F' || memtype == 'f') || size <= 0 || size > 256) {
                resp_failed();
                break;
            }

            put(Resp_STK_INSYNC);
            for (int off = 0; off < size; off++) {
                uint16_t w = avr_read_program_memory((uint16_t)(current_address + (uint32_t)(off / 2)));
                uint8_t b = (off & 1) ? (uint8_t)(w >> 8) : (uint8_t)(w & 0xFF);
                tud_cdc_write_char(b);
            }
            put(Resp_STK_OK);
            flush();
            current_address += (uint32_t)((size + 1) / 2);
        } break;

        default: {
            resp_failed();
        } break;
    }
}

void stk500v1_init(void) {
    current_address = 0;
    programming = false;
    page_size_bytes = 128;
    words_per_page = page_size_bytes / 2;
    rx_len = 0;
}

// Feed bytes from CDC into the framed parser.
void stk500v1_feed(const uint8_t* data, int len) {
    if (!data || len <= 0) return;

    // Append to RX buffer (drop overflowed tail).
    size_t to_copy = (size_t)len;
    if (to_copy > (STK_RX_BUF_SIZE - rx_len)) {
        to_copy = STK_RX_BUF_SIZE - rx_len;
    }
    if (to_copy > 0) {
        memcpy(rx_buf + rx_len, data, to_copy);
        rx_len += to_copy;
    }

    // Parse as many complete frames as possible.
    while (rx_len > 0) {
        // Drop stray EOP bytes (common after previous desync).
        if (rx_buf[0] == Sync_CRC_EOP) {
            drop_rx(1);
            continue;
        }

        uint8_t cmd = rx_buf[0];
        size_t needed = 0;

        switch (cmd) {
            case Cmnd_STK_GET_SYNC:
            case Cmnd_STK_GET_SIGN_ON:
            case Cmnd_STK_ENTER_PROGMODE:
            case Cmnd_STK_LEAVE_PROGMODE:
            case Cmnd_STK_CHIP_ERASE:
            case Cmnd_STK_CHECK_AUTOINC:
            case Cmnd_STK_READ_SIGN:
                needed = 1 + 1; // cmd + EOP
                break;

            case Cmnd_STK_GET_PARAMETER:
                needed = 1 + 1 + 1; // cmd + param + EOP
                break;

            case Cmnd_STK_SET_PARAMETER:
                needed = 1 + 2 + 1; // cmd + (param,value) + EOP
                break;

            case Cmnd_STK_SET_DEVICE:
                needed = 1 + 20 + 1; // cmd + 20 + EOP
                break;

            case Cmnd_STK_SET_DEVICE_EXT:
                needed = 1 + 5 + 1; // cmd + 5 + EOP
                break;

            case Cmnd_STK_LOAD_ADDRESS:
                needed = 1 + 2 + 1; // cmd + (low,high) + EOP
                break;

            case Cmnd_STK_UNIVERSAL:
                needed = 1 + 4 + 1; // cmd + 4 + EOP
                break;

            case Cmnd_STK_READ_PAGE:
                needed = 1 + 3 + 1; // cmd + (size_hi,size_lo,memtype) + EOP
                break;

            case Cmnd_STK_PROG_PAGE: {
                if (rx_len < 4) {
                    // Need cmd + size_hi + size_lo + memtype before we can know total.
                    return;
                }
                int size = ((int)rx_buf[1] << 8) | rx_buf[2];
                if (size < 0 || size > 256) {
                    // Desync; drop one byte and try again.
                    drop_rx(1);
                    continue;
                }
                needed = 1 + 3 + (size_t)size + 1;
            } break;

            default:
                // Unknown / desynced; drop one byte and retry.
                drop_rx(1);
                continue;
        }

        if (needed == 0) {
            drop_rx(1);
            continue;
        }
        if (rx_len < needed) {
            return; // wait for more bytes
        }
        if (rx_buf[needed - 1] != Sync_CRC_EOP) {
            // Not a valid frame; resync.
            // If there's an EOP somewhere, drop up to it; otherwise drop one byte.
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

        // Valid frame: cmd + payload + EOP.
        const uint8_t* payload = rx_buf + 1;
        size_t payload_len = needed - 2;
        (void)programming;
        handle_frame(cmd, payload, payload_len);
        drop_rx(needed);
    }
}
