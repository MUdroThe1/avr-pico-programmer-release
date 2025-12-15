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

static inline void put(uint8_t b) { tud_cdc_write_char(b); }
static inline void flush(void) { tud_cdc_write_flush(); }

static inline void resp_ok_insync(void) { put(Resp_STK_INSYNC); put(Resp_STK_OK); flush(); }
static inline void resp_failed(void) { put(Resp_STK_INSYNC); put(Resp_STK_FAILED); flush(); }

void stk500v1_init(void) {
    current_address = 0;
    programming = false;
    page_size_bytes = 128;
    words_per_page = page_size_bytes / 2;
}

// Helper: read next N bytes from CDC (blocking until available or timeout)
static int read_exact(uint8_t* buf, int n, uint32_t timeout_ms) {
    absolute_time_t until = make_timeout_time_ms(timeout_ms);
    int got = 0;
    while (got < n) {
        tud_task();
        int avail = tud_cdc_available();
        if (avail > 0) {
            got += tud_cdc_read(buf + got, n - got);
        } else if (absolute_time_diff_us(get_absolute_time(), until) <= 0) {
            break;
        }
    }
    return got;
}

// Handle the minimal subset that avrdude uses for fuse/signature and flash
void stk500v1_feed(const uint8_t* data, int len) {
    for (int i = 0; i < len; i++) {
        uint8_t cmd = data[i];
        switch (cmd) {
            case Cmnd_STK_GET_SYNC: {
                // Expect Sync_CRC_EOP next; many avrdude send it separately, tolerate missing
                put(Resp_STK_INSYNC);
                put(Resp_STK_OK);
                flush();
            } break;

            case Cmnd_STK_ENTER_PROGMODE: {
                if (avr_enter_programming_mode()) {
                    programming = true;
                    // Detect device and cache page size
                    uint8_t sig[3] = {0};
                    avr_read_signature(sig);
                    const avr_device_t* dev = avr_lookup_device_by_signature(sig);
                    if (dev && dev->page_size_bytes) {
                        page_size_bytes = dev->page_size_bytes;
                        words_per_page = page_size_bytes / 2;
                    }
                    resp_ok_insync();
                } else {
                    resp_failed();
                }
            } break;

            case Cmnd_STK_LEAVE_PROGMODE: {
                programming = false;
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

            case Cmnd_STK_CHIP_ERASE: {
                avr_erase_memory();
                resp_ok_insync();
            } break;

            case Cmnd_STK_CHECK_AUTOINC: {
                put(Resp_STK_INSYNC);
                put(1); // support autoinc
                put(Resp_STK_OK);
                flush();
            } break;

            case Cmnd_STK_LOAD_ADDRESS: {
                // Next two bytes: address low, high (word address)
                uint8_t addr_lo = 0, addr_hi = 0;
                uint8_t tmp[2];
                if (read_exact(tmp, 2, 50) == 2) {
                    addr_lo = tmp[0];
                    addr_hi = tmp[1];
                    current_address = ((uint32_t)addr_hi << 8) | addr_lo; // word address
                    resp_ok_insync();
                } else {
                    resp_failed();
                }
            } break;

            case Cmnd_STK_PROG_PAGE: {
                // Payload: size_hi, size_lo, memtype ('F'/'f' for flash), then data, then EOP
                uint8_t hdr[2];
                if (read_exact(hdr, 2, 100) != 2) { resp_failed(); break; }
                int size = (hdr[0] << 8) | hdr[1];
                uint8_t memtype = 0;
                if (read_exact(&memtype, 1, 50) != 1) { resp_failed(); break; }
                if (!(memtype == 'F' || memtype == 'f')) { resp_failed(); break; }
                if (size <= 0 || size > (int)(page_size_bytes)) { resp_failed(); break; }

                uint8_t buf[256];
                if (size > (int)sizeof(buf)) { resp_failed(); break; }
                if (read_exact(buf, size, 1000) != size) { resp_failed(); break; }
                uint8_t eop;
                if (read_exact(&eop, 1, 50) != 1 || eop != Sync_CRC_EOP) { resp_failed(); break; }

                int words = size / 2;
                // Write page buffer words starting at offset 0 for this page
                for (int j = 0; j < words; j++) {
                    uint16_t low = buf[j * 2 + 0];
                    uint16_t high = buf[j * 2 + 1];
                    uint16_t word = (high << 8) | low;
                    avr_write_temporary_buffer_16((uint16_t)j, word);
                }
                // Commit page at current word address
                avr_flash_program_memory((uint16_t)current_address);
                // Auto-increment by words written
                current_address += (uint32_t)words;
                resp_ok_insync();
            } break;

            case Cmnd_STK_READ_PAGE: {
                // Payload: size_hi, size_lo, memtype ('F'/'f'), then EOP
                uint8_t hdr[2];
                if (read_exact(hdr, 2, 100) != 2) { resp_failed(); break; }
                int size = (hdr[0] << 8) | hdr[1];
                uint8_t memtype = 0;
                if (read_exact(&memtype, 1, 50) != 1) { resp_failed(); break; }
                uint8_t eop;
                if (read_exact(&eop, 1, 50) != 1 || eop != Sync_CRC_EOP) { resp_failed(); break; }
                if (!(memtype == 'F' || memtype == 'f')) { resp_failed(); break; }
                if (size <= 0 || size > 256) { resp_failed(); break; }

                put(Resp_STK_INSYNC);
                // Read 'size' bytes starting at current address (word -> bytes)
                for (int off = 0; off < size; off += 2) {
                    uint16_t w = avr_read_program_memory((uint16_t)(current_address + (off/2)));
                    uint8_t low = (uint8_t)(w & 0xFF);
                    uint8_t high = (uint8_t)(w >> 8);
                    tud_cdc_write_char(low);
                    tud_cdc_write_char(high);
                }
                put(Resp_STK_OK);
                flush();
                current_address += (uint32_t)(size / 2);
            } break;

            case Cmnd_STK_UNIVERSAL: {
                // 4-byte SPI payload: commonly used to read fuses via SPI commands
                uint8_t u[4];
                if (read_exact(u, 4, 100) != 4) { resp_failed(); break; }
                uint8_t eop;
                if (read_exact(&eop, 1, 50) != 1 || eop != Sync_CRC_EOP) { resp_failed(); break; }
                // Map universal to avr_transfer (send 4 bytes, get last byte)
                uint8_t tx[4] = {u[0], u[1], u[2], u[3]};
                uint8_t rx[4] = {0};
                spi_write_read_blocking(spi0, tx, rx, 4);
                uint8_t res = rx[3];
                put(Resp_STK_INSYNC);
                put(res);
                put(Resp_STK_OK);
                flush();
            } break;

            default: {
                // Unknown; try to consume until EOP if present
                resp_failed();
            } break;
        }
    }
}
