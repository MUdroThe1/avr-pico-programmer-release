# RP2040 AVR ISP (STK500v1) Programmer

This firmware turns a Raspberry Pi Pico (RP2040) into an AVR ISP programmer that speaks a minimal STK500v1 ("arduino") protocol over USB CDC. It supports reading signature and fuses (via UNIVERSAL command), chip erase, page writes/reads of flash, and works with avrdude.

- Protocol: STK500v1 subset (GET_SYNC, ENTER/LEAVE_PROGMODE, LOAD_ADDRESS, PROG_PAGE, READ_PAGE, CHIP_ERASE, UNIVERSAL)
- Transport: USB CDC (ttyACM)
- Backend: Hardware SPI using RP2040's SPI peripheral via `avrprog.*`

## ⚠️ Work In Progress

### Bitbanging Feature
A **bitbanging mode** is currently **in development**. This feature will allow:
- Software-based SPI implementation for maximum pin flexibility
- Support for non-standard pin configurations
- Fallback mode when hardware SPI is unavailable

*Status: In progress - not yet functional*

### Protocol Trace Parser (`test.py`)
The `pico/test.py` Python script is a **work-in-progress** tool for parsing and analyzing STK500v1 protocol traces. Planned features include:
- Full command decoding with human-readable output
- Response validation and error detection
- Flash data hex dump and analysis
- Protocol timing analysis for debugging

*Status: In progress - basic frame parsing implemented*

## Wiring (default pins)

The current pin mapping is defined in `pico/avrprog.c`:

- `MOSI` → GPIO 19
- `MISO` → GPIO 16
- `SCK`  → GPIO 18
- `RESET`→ GPIO 17 (active low)

Target connections (AVR 6‑pin ISP header):

- 1: MISO ↔ GPIO 16 (level-shifted if target is 5V)
- 2: VCC (target power)
- 3: SCK  ↔ GPIO 18
- 4: MOSI ↔ GPIO 19
- 5: RESET↔ GPIO 17
- 6: GND

Note: RP2040 is 3.3V. Use a proper level shifter when the AVR runs at 5V.

## Build

This project uses Pico SDK + TinyUSB. From the `avr-pico-programmer-release` folder:

```zsh
mkdir -p build
cd build
cmake -DPICO_SDK_PATH="../pico-sdk" -DPICO_EXTRAS_PATH="../pico-extras" ../pico
make -j
```

Copy the generated UF2 to the Pico (BOOTSEL mode) to flash the firmware.

## Verify USB device

After flashing, the device should enumerate as `/dev/ttyACM0` (or similar):

```zsh
ls /dev/ttyACM*
```

## Using avrdude

Example device: ATmega328P (`-p m328p`). Baudrate is not used by USB CDC but avrdude requires one for `-c arduino`; 115200 is fine.

- Probe signature and basic communication:
```zsh
avrdude -p m328p -c arduino -P /dev/ttyACM0 -b 115200 -v -n
```

- Read fuses and lock:
```zsh
avrdude -p m328p -c arduino -P /dev/ttyACM0 -b 115200 \
  -U lfuse:r:-:h -U hfuse:r:-:h -U efuse:r:-:h -U lock:r:-:h
```

- Erase + flash a HEX file:
```zsh
avrdude -p m328p -c arduino -P /dev/ttyACM0 -b 115200 -e -U flash:w:/path/to/firmware.hex:i
```

- Read back a flash page (example 128 bytes at current address): avrdude handles paging internally; use:
```zsh
avrdude -p m328p -c arduino -P /dev/ttyACM0 -b 115200 -U flash:r:/tmp/readback.hex:i
```

## Notes

- Page size is auto-detected from the signature using `pico/avr_devices.c`. If your device is unknown, add its signature, name, and `page_size_bytes` there.
- The firmware implements `UNIVERSAL` via raw 4‑byte SPI, so avrdude can read fuses using standard sequences.
- If you see `programmer is not responding`:
  - Ensure the target has power and correct clock source.
  - Check RESET wiring and that the AVR is not held in reset by the board.
  - Verify level shifting if using a 5V target.

## Changing pins or SPI speed

Pins and SPI frequency are currently hardcoded in `pico/avrprog.c` (`spi_init(spi0, 200000)`). Adjust as needed, and rebuild.
