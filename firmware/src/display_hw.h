// OLED drawing for the chip. UNTESTED: written without the screen, so expect small fixes.
// Only compiled on the chip. Uses the same text lines as the computer tests (screen_text.h).
#pragma once
#ifdef ARDUINO
#include <U8g2lib.h>
#include "ssd1317_128x64.h"
#include <string.h>
#include "ui.h"
#include "screen_text.h"

namespace hw {

// Longest calculator row: 12 characters plus the end marker.
constexpr uint8_t ROW_BUF = 13;

// The controller constructor. The screen is a 128x64 panel with an SSD1317 controller (I2C).
// The driver is in ssd1317_128x64.h. Not verified on the screen.
// If the screen does not light up or the image is wrong, the init sequence there is the place to check.
static U8G2_SSD1317_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

inline void begin() {
  u8g2.begin();
  u8g2.setContrast(200);
}

// Fonts per calculator row count. Names are from the U8g2 font list; check them at compile time.
// logisoso38 and logisoso30 and logisoso20 are confirmed in the list; logisoso62 is not, so the
// 1-row size uses logisoso38 until the 62-pixel font is confirmed.
inline const uint8_t* calcFont(uint8_t rows) {
  if (rows == 1) return u8g2_font_logisoso38_tr;
  if (rows == 2) return u8g2_font_logisoso30_tr;
  return u8g2_font_logisoso20_tr;
}

// Calculator screen: text rows fill the height. Last row right-aligned, others left.
inline void drawCalc(const char* text, bool isError, bool unread) {
  u8g2.clearBuffer();
  if (unread) u8g2.drawBox(121, 0, 7, 7);          // unread square, top right

  // Collect the rows first, so we know which one is last.
  char rows[3][ROW_BUF];
  uint8_t count = 0;
  screen::forEachCalcLine(text, isError, [&](const char* line) {
    if (count < 3) { strncpy(rows[count], line, ROW_BUF - 1); rows[count][ROW_BUF - 1] = 0; }
    count++;
  });
  if (count > 3) count = 3;

  uint8_t rowsUsed = count;
  uint8_t rowH = 64 / (rowsUsed == 0 ? 1 : rowsUsed);
  u8g2.setFont(calcFont(rowsUsed));
  for (uint8_t i = 0; i < count; i++) {
    // trim the padding the text model adds
    const char* s = rows[i];
    while (*s == ' ') s++;
    char trimmed[ROW_BUF];
    strncpy(trimmed, s, sizeof trimmed - 1);
    trimmed[sizeof trimmed - 1] = 0;
    size_t tl = strlen(trimmed);
    while (tl && trimmed[tl - 1] == ' ') trimmed[--tl] = 0;
    int w = u8g2.getUTF8Width(trimmed);
    bool last = (i == count - 1);
    int x = last ? (128 - w) : 0;
    int baseline = (i + 1) * rowH - 2;
    u8g2.drawUTF8(x < 0 ? 0 : x, baseline, trimmed);
  }
  u8g2.sendBuffer();
}

// Contacts: one row per contact. * is the selected row; the box is unread.
inline void drawContacts(const ui::App& a) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  for (uint8_t r = 0; r < ui::App::NUM_CONTACTS; r++) {
    uint8_t slot = ui::App::slotForRow(r);
    char name[12];
    if (slot == comm::ALL_ID) snprintf(name, sizeof name, "ALL");
    else snprintf(name, sizeof name, "CALC %u", (unsigned)slot);
    int y = 14 + r * 16;
    if (r == a.sel) u8g2.drawStr(0, y, "*");
    u8g2.drawStr(12, y, name);
    if (a.rowUnread(r)) u8g2.drawBox(116, y - 8, 7, 7);
  }
  u8g2.sendBuffer();
}

// Chat: header with the chat name and unread square, then the bubbles. Uses ASCII
// bubble edges until the bubble drawing is done with real rounded boxes.
inline void drawChat(const ui::App& a) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tf);
  char header[24] = "";
  screen::forEachChatHeader(a, [&](const char* s) {
    strncpy(header, s, sizeof header - 1);
    header[sizeof header - 1] = 0;
  });
  for (char* p = header; *p; p++) if (*p == '#') *p = ' ';
  u8g2.drawStr(0, 7, header);
  if (a.msg.hasUnread()) u8g2.drawBox(121, 0, 7, 7);

  int row = 0;
  screen::forEachChatLine(a, [&](const char* s) {
    u8g2.drawStr(0, 16 + row * 8 + 7, s);
    row++;
  });
  u8g2.sendBuffer();
}

}  // namespace hw
#endif  // ARDUINO
