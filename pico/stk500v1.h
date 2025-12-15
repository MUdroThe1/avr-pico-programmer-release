#pragma once
#include <stdint.h>

// Minimal STK500v1 command set used by avrdude -c arduino
#define Cmnd_STK_GET_SYNC         0x30
#define Cmnd_STK_GET_SIGN_ON      0x31
#define Cmnd_STK_SET_PARAMETER    0x40
#define Cmnd_STK_ENTER_PROGMODE   0x50
#define Cmnd_STK_LEAVE_PROGMODE   0x51
#define Cmnd_STK_CHIP_ERASE       0x52
#define Cmnd_STK_CHECK_AUTOINC    0x53
#define Cmnd_STK_LOAD_ADDRESS     0x55
#define Cmnd_STK_UNIVERSAL        0x56
#define Cmnd_STK_PROG_PAGE        0x64
#define Cmnd_STK_READ_PAGE        0x74
#define Cmnd_STK_PROG_FLASH       0x60  // not used (byte write), kept for completeness
#define Cmnd_STK_PROG_DATA        0x61  // not used
#define Cmnd_STK_READ_FLASH       0x70  // not used (byte read)
#define Cmnd_STK_READ_DATA        0x71  // not used
#define Cmnd_STK_READ_SIGN        0x75

#define Sync_CRC_EOP              0x20

#define Resp_STK_INSYNC           0x14
#define Resp_STK_OK               0x10
#define Resp_STK_NOSYNC           0x15
#define Resp_STK_FAILED           0x11

void stk500v1_init(void);
void stk500v1_feed(const uint8_t* data, int len);
