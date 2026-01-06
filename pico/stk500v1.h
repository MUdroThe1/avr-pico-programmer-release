/**
 * @file stk500v1.h
 * @brief STK500v1 Protocol Constants and Interface Definitions
 * 
 * This header defines the constants and function prototypes for the
 * STK500v1 ("arduino") protocol implementation. STK500v1 is the protocol
 * used by Arduino bootloaders and is supported by avrdude.
 * 
 * Protocol Structure:
 *   - Commands end with Sync_CRC_EOP (0x20)
 *   - Responses start with Resp_STK_INSYNC (0x14)
 *   - Success ends with Resp_STK_OK (0x10)
 *   - Failure ends with Resp_STK_FAILED (0x11)
 * 
 * Reference: Atmel AVR061 - STK500 Communication Protocol
 * 
 * @author MUdroThe1
 * @date 2026
 */

#pragma once
#include <stdint.h>

/*******************************************************************************
 * STK500v1 Command Definitions
 * These are the command bytes sent by avrdude/host to the programmer
 ******************************************************************************/

/* Synchronization and identification commands */
#define Cmnd_STK_GET_SYNC         0x30  /* Sync check - programmer responds if alive */
#define Cmnd_STK_GET_SIGN_ON      0x31  /* Get programmer identification string */

/* Parameter commands */
#define Cmnd_STK_SET_PARAMETER    0x40  /* Set a programmer parameter */
#define Cmnd_STK_GET_PARAMETER    0x41  /* Get a programmer parameter value */

/* Device configuration commands */
#define Cmnd_STK_SET_DEVICE       0x42  /* Set target device parameters (20 bytes) */
#define Cmnd_STK_SET_DEVICE_EXT   0x45  /* Set extended device parameters (5 bytes) */

/* Programming mode control commands */
#define Cmnd_STK_ENTER_PROGMODE   0x50  /* Enter programming mode (assert RESET) */
#define Cmnd_STK_LEAVE_PROGMODE   0x51  /* Leave programming mode (release RESET) */
#define Cmnd_STK_CHIP_ERASE       0x52  /* Erase entire flash memory */
#define Cmnd_STK_CHECK_AUTOINC    0x53  /* Check if address auto-increments */

/* Address and data commands */
#define Cmnd_STK_LOAD_ADDRESS     0x55  /* Set current read/write address (word) */
#define Cmnd_STK_UNIVERSAL        0x56  /* Raw 4-byte SPI transaction */

/* Flash programming commands */
#define Cmnd_STK_PROG_PAGE        0x64  /* Program a page of flash/EEPROM */
#define Cmnd_STK_READ_PAGE        0x74  /* Read a page of flash/EEPROM */

/* Legacy byte-at-a-time commands (not implemented) */
#define Cmnd_STK_PROG_FLASH       0x60  /* Program single flash byte (unused) */
#define Cmnd_STK_PROG_DATA        0x61  /* Program EEPROM byte (unused) */
#define Cmnd_STK_READ_FLASH       0x70  /* Read single flash byte (unused) */
#define Cmnd_STK_READ_DATA        0x71  /* Read EEPROM byte (unused) */

/* Signature reading */
#define Cmnd_STK_READ_SIGN        0x75  /* Read target device signature (3 bytes) */

/*******************************************************************************
 * STK500v1 Framing and Response Codes
 ******************************************************************************/

#define Sync_CRC_EOP              0x20  /* End-of-packet marker (space character) */

#define Resp_STK_INSYNC           0x14  /* Response: programmer is synchronized */
#define Resp_STK_OK               0x10  /* Response: command executed successfully */
#define Resp_STK_NOSYNC           0x15  /* Response: framing error, not in sync */
#define Resp_STK_FAILED           0x11  /* Response: command execution failed */

/*******************************************************************************
 * Function Prototypes
 ******************************************************************************/

/**
 * @brief Initialize STK500v1 protocol handler
 * 
 * Resets all protocol state to initial values.
 * Call once at startup before entering main loop.
 */
void stk500v1_init(void);

/**
 * @brief Feed received bytes to protocol parser
 * 
 * Call this function whenever bytes are received from USB CDC.
 * The parser will accumulate bytes and process complete command frames.
 * 
 * @param data Pointer to received byte buffer
 * @param len  Number of bytes in buffer
 */
void stk500v1_feed(const uint8_t* data, int len);
