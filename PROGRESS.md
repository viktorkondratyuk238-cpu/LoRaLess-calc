# LoRaLess calc: progress summary

Status as of 2026-10-30. Nothing has run on the physical board yet.

## What has been done

### Schematic and BOM
- Read the KiCad schematic, the JLCPCB BOM and all firmware files.
- Every reference designator in the BOM is placed on the schematic. Two placed parts are not in the BOM: J2 (1x04 pin header, the screen connector) and J3 (LiPo cable).
- Three parts are only in the library cache, not placed: CH340C, SX1262, ESP32-C3-DevKitM-1.
- The BOM lists R1 and R2 as `5,1k` (comma decimal). The value is right, but `5.1k` is safer for the fabricator.
- Ran a wiring check (`firmware/tools/check_board.py`, 28 checks). All pass:
  - I2C: ESP32 IO4 to MCP23017 SDA (pin 13), IO5 to SCL (pin 12).
  - MCP23017 address 0x20: A0, A1, A2 tied to GND. VDD on 3.3 V, VSS on GND, RESET held high.
  - Screen header J2 pin order is GND, VCC, SCL, SDA. SCL is IO5 and SDA is IO4.
  - Keypad: 6 rows (GPA0 to GPA5) by 5 columns (GPB0 to GPB4). All 30 keys (SW1 to SW30) are placed, each with a unique row and column.
  - The unused expander pins (GPA6, GPA7, GPB5, GPB6, GPB7) are not connected to anything, so driving them low is safe.
  - SW31 appears to switch the battery supply, but I haven't traced it fully.

### Firmware build
- Moved the chip source files into `firmware/src/`, which PlatformIO and the test commands expect.
- Pinned the platform to `espressif32@6.9.0`. The newest platform fails to build with `-DARDUINO_USB_CDC_ON_BOOT=1` ("USBSerial was not declared").
- Added `-DARDUINO_USB_MODE=1` to `platformio.ini`.
- `pio run` now builds `calc1`, `calc2`, `calc3` and `calc4` successfully. This is a compile check only.

### Host tests (run on the PC, no board)
- `test.cpp`: 16 of 16 calculator cases pass, with real pass/fail checks.
- `test_main.cpp`: 23 passed, 0 failed.
- `test_ui.cpp`: 47 passed, 0 failed.
- `test_screen.cpp`: 31 passed, 0 failed.
- `test_comm.cpp`: 53 passed, 0 failed.
- `run_tests.sh` runs all of them for DEVICE_ID 1 to 4: 20 of 20 runs pass. The tests pick the other boards with `peer(k)` (in `test_util.h`), not fixed numbers.
- Checked that the tests can fail: a wrong error message and a moved unused-pin entry each made the matching test fail.
- The Windows Smart App Control setting was blocking the test programs. It has been turned off.

### Screen
- The screen is a 1.54-inch 128x64 panel with an SSD1317 controller (I2C). U8g2's SSD1317 driver only supports 96x96 panels, so there is no matching driver.
- `firmware/src/ssd1317_128x64.h` is our SSD1317 128x64 driver, copied from U8g2's 96x96 SSD1317 driver with the init changed for 128x64. It builds for calc1. The init is from the datasheet command table (Rev 1.2) but is unverified on the screen.

### Tools installed on this PC (outside the project)
- Python 3.12.10
- PlatformIO 6.2.0
- WinLibs MinGW g++ 16.2
- MSYS2 with the UCRT64 g++ toolchain (used for the host tests)

## What is still to do

### Firmware
- **Display:** the screen is untested. If it stays blank or the image is wrong, check the init sequence in `firmware/src/ssd1317_128x64.h` (segment/COM remap and COM pin config are the likely ones).
- **Fonts:** logisoso62 is not confirmed, so the 1-row size uses logisoso38.
- **Character widths:** `CALC_LEVELS`, `COLS` and `BUBBLE_TEXT` are placeholders until measured on the real screen.
- **Chat bubbles:** still ASCII edges. Rounded boxes are not done.
- **ESP-NOW:** the radio code has never run on hardware. Two boards need to send and receive a message.
- **Keypad:** the scan and debounce logic has only been tested against fakes.
- **Not yet tested on the PC:** calculator memory keys (MEM W, MR, MC), keypad column digits 0 to 2, and the debounce logic.

### Hardware and PCB
- **Bench parts:** not yet bought. Needed are two ESP32-C3 dev boards, one MCP23017 breakout, one SSD1317 4-pin I2C OLED, a few switches and diodes, breadboard and jumper wires.
- **Power and charging:** the charger (U3), regulator (U5), USB-C input (J1) and power switch (SW31) are untested.
- **Keypad:** the full 30-key matrix has not been tested with real key presses.
- **BOM:** decide whether J2 (pin header) and J3 (LiPo cable) should be added.
- **Schematic cleanup:** remove or confirm the unplaced library parts (CH340C, SX1262, ESP32-C3-DevKitM-1, Q_NPN).
- **Electrical checks:** the schematic has not been run through KiCad's electrical rules check (ERC) yet.

### Tools and docs
- `firmware/tools/netcheck.py` expects `lora-calculator_jlcpcb_updated.kicad_sch`, but the file is `lora-calculator_jlcpcb.kicad_sch`. The README says to run it from the folder with the `_updated` name. The script still needs fixing (or the file renaming).
- `firmware/README.txt` has been updated for the display and the `src/` layout. Its test commands are correct.

## Suggested bring-up order (once parts are in)
1. Flash `calc1` and check the serial output over USB.
2. Run an I2C scan to confirm the MCP23017 at 0x20 and the screen at 0x3C or 0x3D.
3. Test each key press, starting with the four corner keys, and confirm the serial output shows the right key.
4. Get the screen drawing text, then tune the font sizes and character widths.
5. Flash two boards and send one message between them.
