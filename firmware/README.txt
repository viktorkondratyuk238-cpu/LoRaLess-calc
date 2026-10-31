Firmware for the LoRa-less calculator (ESP32-C3), as written so far.

Chip code (goes on the board, build with PlatformIO: pio run -e calc1 ... calc4):
  main.cpp          keypad scan, calculator, key routing, output
  ui.h              which screen is showing and what each key does
  comm.h            messages, chats, multi-tap typing, composer (F<->D, COMP, x2, sqrt)
  screen_text.h     text lines for the calculator, contacts and chat screens
  display_hw.h      OLED drawing (chip only) - UNTESTED
  radio_hw.h        ESP-NOW send and receive (chip only) - UNTESTED
  platformio.ini    board settings, libraries, one build per board

Tests (run on a computer, no board needed):
  test.cpp          calculator cases from the handoff summary (16)
  test_main.cpp     keypad routing, layout, sign and error checks (23)
  test_ui.cpp       screen logic tests (47)
  test_screen.cpp   screen text tests (31)
  test_comm.cpp     messaging tests (53)
  test_util.h       helper: peer(k) gives the other boards' ids, so tests work for any DEVICE_ID
  run_tests.sh      builds and runs every test for DEVICE_ID 1 to 4
  stubs/            fake Arduino, Wire and MCP23017 headers so the tests run on a computer

Run all tests for every board:
  bash run_tests.sh

Build and run one test, for example:
  g++ -std=c++17 -DDEVICE_ID=1 -Istubs -Isrc -o test_comm test_comm.cpp && ./test_comm

Tools:
  tools/netcheck.py reads the KiCad schematic and reports which pins connect to which nets.
  Run it from the folder holding lora-calculator_jlcpcb_updated.kicad_sch.

Open items (need the hardware or a PlatformIO build):
  - Display font names: logisoso62 is not confirmed, so the 1-row size uses logisoso38.
  - Display controller: the screen is a 128x64 panel with an SSD1317 controller (I2C).
    ssd1317_128x64.h is our own 128x64 driver (untested).
    Not verified on the screen; change one line if it fails.
  - Chat bubbles use ASCII edges for now (rounded boxes later).
  - Character widths (CALC_LEVELS, COLS, BUBBLE_TEXT) are placeholders until measured.
  - ESP-NOW and OLED code is untested on the board.
  - Compile with PlatformIO to confirm libraries and names.
