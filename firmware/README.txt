Firmware for the LoRa-less calculator (ESP32-C3), as written so far.

Chip code (goes on the board, build with PlatformIO: pio run -e calc1 ... calc4):
  main.cpp          keypad scan, calculator, key routing, output
  ui.h              which screen is showing and what each key does
  comm.h            messages, chats, multi-tap typing, composer (F<->D, COMP, x2, sqrt)
  screen_text.h     text lines for the calculator, contacts and chat screens
  display_hw.h      OLED drawing (chip only) - UNTESTED
  radio_hw.h        ESP-NOW send and receive (chip only) - UNTESTED
  platformio.ini    board settings, libraries, one build per board

Keypad layout (row 1 at top, column 1 at left; from main.cpp KEYMAP):
  Row 1:  C     DEL    +/-    MEMW   COMMA
  Row 2:  x2    SQRT   ^      MR     MC
  Row 3:  7     8      9      COMP   F<->D
  Row 4:  4     5      6      (      )
  Row 5:  1     2      3      *      /
  Row 6:  0     .      =      +      -

Key by key, calculator screen (main.cpp, ui.h):
  0-9, .      type a number. Typing after a result starts a new calculation.
  + - * / ^   add an operator. After a result, the operator chains from that result.
  ( )         add a bracket.
  =           evaluate the expression and show the result.
  C           clear the calculation.
  DEL         delete the last digit typed, or the last token if no digits are typed.
  +/-         flip the sign of the number being typed, or of the last result.
  x2          square the last number or result.
  SQRT        square root of the last number or result.
  MEMW        store the number being typed, or the last result, in memory.
  MR          recall memory into the calculation (only if memory is set).
  MC          clear memory.
  COMP        scale the last fraction by a whole number (type the number, then =).
  F<->D       toggle fraction / decimal display (only after an exact result).
  COMMA       open the contacts screen.

Key by key, contacts screen (ui.h):
  ( )         move the selection up / down.
  =           open the selected chat.
  COMMA       back to the calculator.

Key by key, chat screen (ui.h, comm.h):
  0-9         multi-tap letters. Press the same key again within 1 second to cycle
              its letters (digit 1: 1 . , ? !; 2: 2 A B C; ... 9: 9 W X Y Z; 0: 0 and space).
              Press a different key, or wait over 1 second, to start the next letter.
  . + - * / ^ types that symbol (the minus, times and divide signs are drawn as glyphs).
  x2 / SQRT   replace the last number in the message with its square / square root.
  COMP        start a COMP scale on the last fraction; type a whole number, then =.
  F<->D       toggle fraction / decimal display.
  =           press twice within 0.5 s to send. One press types an =.
  DEL         delete the last character.
  C           clear the message.
  ( )         scroll the chat up / down (any other key returns to the bottom).
  COMMA       leave the chat, back to the calculator.
  MEMW / MR / MC   not used in chat.
  Messages are up to 120 characters.

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
