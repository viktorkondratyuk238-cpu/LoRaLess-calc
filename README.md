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
- **Extra buttons:** SW32 is reset, SW33 is boot. SW31 is a power switch.

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

- The screen is untested. U8g2's SSD1317 driver only supports 96 × 96 panels, so the firmware uses the SSD1306 128 × 64 driver as a stand-in. The panel is 128 × 64, so the stand-in matches its size but not its controller.
- The ESP-NOW radio code and the keypad have not run on hardware.
- `netcheck.py` expects a schematic file called `lora-calculator_jlcpcb_updated.kicad_sch`. The file in this repo is `lora-calculator_jlcpcb.kicad_sch`.
- The screen connector J2 and the LiPo cable J3 are not in the BOM.

Full list: [PROGRESS.md](PROGRESS.md).
