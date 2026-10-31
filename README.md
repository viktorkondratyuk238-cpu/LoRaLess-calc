# LoRaLess calc

A handheld calculator built around an ESP32-C3. It has a keypad, a small OLED screen, and it passes messages to other calculators over ESP-NOW (the chip's built-in radio, not LoRa).

**Status:** the design and firmware are in progress. The firmware compiles and its logic tests pass on a PC. Nothing has run on the physical board yet. See [PROGRESS.md](PROGRESS.md) for what is done and what is still open.

## Contents

| Path | What it is |
|---|---|
| `lora-calculator_jlcpcb.kicad_sch` | KiCad schematic of the PCB |
| `bom_jlcpcb.csv` | Bill of materials for JLCPCB assembly |
| `firmware/src/` | Chip firmware: keypad, calculator, screen, messaging, ESP-NOW |
| `firmware/test*.cpp` | Logic tests that run on a PC, with no board needed |
| `firmware/stubs/` | Fake Arduino, Wire and MCP23017 headers so the tests run on a PC |
| `firmware/tools/check_board.py` | Checks the schematic wiring against what the firmware expects |
| `firmware/tools/netcheck.py` | Lists which pins connect to which nets in the schematic |
| `firmware/platformio.ini` | Board settings and one build target per calculator (`calc1` to `calc4`) |

## Hardware

- **MCU:** ESP32-C3-WROOM-02U module
- **Keypad:** 30 keys in a 6 × 5 matrix, read through an MCP23017 I/O expander at I2C address 0x20
- **Screen:** 1.54-inch 128 × 64 OLED with an SSD1317 controller, connected over I2C (header J2: GND, VCC, SCL, SDA)
- **Power:** USB-C input, MCP73831 battery charger, AP2112K-3.3 regulator, LiPo cable (J3)
- **Extra button:** SW31 is a power switch.

### Parts not in the PCB BOM

These are not in `bom_jlcpcb.csv` and must be sourced separately:

- **Screen:** 1.54-inch 128 × 64 OLED (SSD1317), I2C, connected to header J2 (GND, VCC, SCL, SDA).
- **Battery:** 500 mAh 1S LiPo (3.7 V nominal), connected to the LiPo cable header J3. The MCP73831 charger on the board charges it over USB-C.
- **Antenna:** 2.4 GHz T-style omnidirectional antenna, 40 mm version (the listing also has a 90 mm version), on a short coax pigtail with a small IPX1 / U.FL-type connector. It is sold for ELRS/TBS receivers. Connect it to the ESP32-C3-WROOM-02U module's external antenna pad (the "U" variant has a U.FL connector for this).

## Keys

Layout (row 1 at top, column 1 at left; from `KEYMAP` in `firmware/src/main.cpp`):

|| Col 1| Col 2 | Col 3 | Col 4 | Col 5 |
|---|---|---|---|---|---|
| Row 1 | C | DEL | +/- | MEMW | COMMA |
| Row 2 | x² | SQRT | ^ | MR | MC |
| Row 3 | 7 | 8 | 9 | COMP | F↔D |
| Row 4 | 4 | 5 | 6 | ( | ) |
| Row 5 | 1 | 2 | 3 | * | / |
| Row 6 | 0 | . | = | + | - |

**Calculator screen**

- `0`-`9`, `.`: type a number. Typing after a result starts a new calculation.
- `+ - * / ^`: add an operator. After a result, it chains from that result.
- `( )`: add a bracket.
- `=`: evaluate and show the result.
- `x²` / `SQRT`: square / square root of the last number or result.
- `C`: clear the calculation. `DEL`: delete the last digit typed, or the last token if no digits are typed.
- `+/-`: flip the sign of the number being typed, or of the last result.
- `COMP`: scale the last fraction by a whole number (type the number, then `=`).
- `F↔D`: toggle fraction / decimal display (only after an exact result).
- `MEMW`: store the number being typed, or the last result, in memory. `MR`: recall memory (only if set). `MC`: clear memory.
- `COMMA`: open the contacts screen.

**Contacts screen**

- `( )`: move the selection up / down.
- `=`: open the selected chat.
- `COMMA`: back to the calculator.

**Chat screen**

- `0`-`9`: multi-tap letters. Press the same key again within 1 second to cycle its letters (1: `1 . , ? !`; 2: `2 A B C`; ... 9: `9 W X Y Z`; 0: `0` and space). Press a different key, or wait over 1 second, for the next letter.
- `. + - * / ^`: type that symbol.
- `x²` / `SQRT`: replace the last number in the message with its square / square root.
- `COMP`: start a COMP scale on the last fraction; type a whole number, then `=`.
- `F↔D`: toggle fraction / decimal display.
- `=`: press twice within 0.5 s to send. One press types an `=`.
- `DEL`: delete the last character. `C`: clear the message.
- `( )`: scroll the chat up / down (any other key returns to the bottom).
- `COMMA`: leave the chat, back to the calculator.
- Messages are up to 120 characters.

## Building the firmware

The firmware uses [PlatformIO](https://platformio.org/). From the `firmware` folder:

```
pio run -e calc1          # build for calculator 1 (calc1 to calc4 are the four boards)
pio run -e calc1 -t upload
```

The platform is pinned to `espressif32@6.9.0` and the USB serial flags are set in `platformio.ini`. The newest platform fails to build this code.

## Running the tests

The tests compile on a PC with g++ (C++17). From the `firmware` folder, run every test for each board (DEVICE_ID 1 to 4):

```
bash run_tests.sh
```

It prints one line per test and exits non-zero if any test fails. To run one test by hand, for example for board 1:

```
g++ -std=c++17 -DDEVICE_ID=1 -Istubs -Isrc -o test_comm test_comm.cpp && ./test_comm
```

## Checking the schematic

Run the wiring checks from the repo root (Python 3):

```
python firmware/tools/check_board.py lora-calculator_jlcpcb.kicad_sch
```

It prints PASS or FAIL for each check and exits with a non-zero code if any fail. These checks confirm the wiring matches the firmware. They can't show that the screen, keys or radio work on a real board.

## Open items

- The screen is untested. U8g2's SSD1317 driver only supports 96 × 96 panels, so the firmware has its own SSD1317 128 × 64 driver (`firmware/src/ssd1317_128x64.h`). The init sequence comes from the datasheet command table, not a vendor sample.
- The ESP-NOW radio code and the keypad have not run on hardware.
- The screen connector J2 and the LiPo cable J3 are not in the BOM (see [Parts not in the PCB BOM](#parts-not-in-the-pcb-bom)).

Full list: [PROGRESS.md](PROGRESS.md).
