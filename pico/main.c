#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/spi.h>
#include "avrprog.h"
#include "avr_devices.h"
#include <stdlib.h>
#include <string.h>

uint8_t program_code[2000] = {};
char input[10] = {0};

int main() {
    stdio_init_all();
    sleep_ms(1000);

    while (true) {
        int code_c = 0;
        int input_c = 0;

        while (true) {
            char c = getchar();

            // Tells the automatic programmer the programmer is ready.
            if (c == '?') {
                printf("READY");
            }

            if (!(c == 13 || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || c == ' ')) {
                continue;
            }

            if (c == 13) {
                input[input_c] = 0;
                break;
            }

            input[input_c] = c;

            if (input[input_c] == ' ') {
                input[input_c] = 0;

                // Readback the byte.
                printf(input);

                // Convert from hex to byte.
                program_code[code_c] = strtol(input, NULL, 16);

                input_c = 0;
                code_c++;
            }

            else input_c++;
        }

        printf("Program length: %d / first & last byte: %hhu %hhu\n", code_c, program_code[0], program_code[code_c - 1]);

        if ((code_c % 2) != 0) {
            printf("program bytes are not a multiple of 2!\n");
            return 0;
        }

        avr_spi_init();
        avr_reset();

        if (!avr_enter_programming_mode()) {
            printf("failed to enter programming mode\n");
            return 0;
        }

        printf("Entered programming mode\n");

        avr_erase_memory();

        printf("Erased program memory successfully for programming\n");

        // Read device signature and lookup device profile
        uint8_t signature[3] = {0};
        avr_read_signature(signature);

        const avr_device_t *dev = avr_lookup_device_by_signature(signature);
        uint16_t page_size_bytes = 64; // default (legacy behaviour)
        if (dev) {
            page_size_bytes = dev->page_size_bytes;
            printf("Detected device: %s (page %d bytes)\n", dev->name, dev->page_size_bytes);
        } else {
            printf("Unknown device signature: %02X %02X %02X - using default page size %d bytes\n",
                   signature[0], signature[1], signature[2], page_size_bytes);
        }

        uint16_t words_per_page = page_size_bytes / 2;

        int full_pages = code_c / page_size_bytes;
        int remaining_bytes = code_c % page_size_bytes;
        int bytes_offset = full_pages * page_size_bytes;

        // buffer for a single page in words
        uint16_t *page_words = malloc(sizeof(uint16_t) * words_per_page);
        if (!page_words) {
            printf("Out of memory allocating page buffer\n");
            return 0;
        }

        // full pages
        for (int i = 0; i < full_pages; i++) {
            printf("Flashing page %d..\n", i);

            int page_offset = i * page_size_bytes;

            // build word buffer (little-endian bytes -> 16-bit words)
            for (int j = 0; j < words_per_page; j++) {
                int byte_index = page_offset + j * 2;
                uint16_t low = program_code[byte_index];
                uint16_t high = program_code[byte_index + 1];
                page_words[j] = (high << 8) | low;
            }

            // write temporary buffer (word by word)
            for (int j = 0; j < words_per_page; j++) {
                avr_write_temporary_buffer_16((uint16_t)j, page_words[j]);
            }

            // commit the page (address in words)
            avr_flash_program_memory(i * words_per_page);

            printf("Verifying flash..\n");

            if (!avr_verify_program_memory_page(i * words_per_page, page_words, words_per_page)) {
                printf("Verification failed! Page has not been flashed correctly..\n");
                free(page_words);
                sleep_ms(100);
                return 0;
            } else printf("Page flash successfully verified!\n");
        }

        // partial page (if any)
        if (remaining_bytes > 0) {
            printf("Flashing partial page..\n");

            int remaining_words = remaining_bytes / 2;

            // fill page buffer with 0xFFFF (erased state)
            for (int j = 0; j < words_per_page; j++) page_words[j] = 0xFFFF;

            // copy remaining data
            for (int j = 0; j < remaining_words; j++) {
                int byte_index = bytes_offset + j * 2;
                uint16_t low = program_code[byte_index];
                uint16_t high = program_code[byte_index + 1];
                page_words[j] = (high << 8) | low;
            }

            // write entire page buffer (device expects full-page commit)
            for (int j = 0; j < words_per_page; j++) {
                avr_write_temporary_buffer_16((uint16_t)j, page_words[j]);
            }

            // commit at next page address (in words)
            avr_flash_program_memory(full_pages * words_per_page);

            printf("Verifying flash..\n");

            if (!avr_verify_program_memory_page(full_pages * words_per_page, page_words, words_per_page)) {
                printf("Verification failed! Partial page has not been flashed correctly..\n");
                free(page_words);
                sleep_ms(100);
                return 0;
            } else printf("Partial page successfully verified!\n");
        }

        // overall verification (word count)
        printf("Verifying overall page flashes..\n");
        int total_words = code_c / 2;
        uint16_t *full_words = malloc(sizeof(uint16_t) * total_words);
        if (!full_words) {
            printf("Out of memory allocating full verification buffer\n");
            free(page_words);
            return 0;
        }
        for (int i = 0; i < total_words; i++) {
            int b = i * 2;
            full_words[i] = (program_code[b+1] << 8) | program_code[b];
        }

        if (!avr_verify_program_memory_page(0, full_words, total_words)) {
            printf("Overall page verification failed! Pages have not been flashed correctly..\n");
            free(page_words);
            free(full_words);
            sleep_ms(100);
            return 0;
        } else printf("Overall page flash verification successful! Firmware has been flashed correctly\n");

        free(page_words);
        free(full_words);

        printf("Going back to standby mode for future flashes..\n");
        printf("FINISH");
    }

    return 0;
}
