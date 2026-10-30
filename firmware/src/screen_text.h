// Text for the contacts and chat screens. It produces lines and does no drawing,
// so the OLED code can print the same lines later. The serial port uses plain ASCII.
#pragma once
#include <stdio.h>
#include <string.h>
#include "ui.h"

namespace screen {

// Plain-ASCII copy of a message (the OLED draws its own glyphs for the special symbols).
inline void toAscii(char* dst, const char* src, size_t n) {
  for (size_t i = 0; i < n; i++) {
    char c = src[i];
    if (c == comm::SYM_MUL) c = 'x';
    else if (c == comm::SYM_DIV) c = '/';
    else if (c == comm::SYM_SUB) c = '-';
    else if (c == comm::SYM_POW) c = '^';
    dst[i] = c;
  }
  dst[n] = 0;
}

// One line per contact: "*" marks the selected row, "#" marks unread
// (the OLED draws a filled square there).
template <typename Emit>
void forEachContactLine(const ui::App& a, Emit emit) {
  char line[24];
  for (uint8_t r = 0; r < ui::App::NUM_CONTACTS; r++) {
    uint8_t slot = ui::App::slotForRow(r);
    char name[12];
    if (slot == comm::ALL_ID) snprintf(name, sizeof name, "ALL");
    else snprintf(name, sizeof name, "CALC %u", (unsigned)slot);
    snprintf(line, sizeof line, "%c %-6s %c", r == a.sel ? '*' : ' ', name, a.rowUnread(r) ? '#' : ' ');
    emit(line);
  }
}

// Places seg at column x of a COLS-wide line (the rest is spaces).
inline void placeAt(char* line, int x, const char* seg, size_t segLen) {
  memset(line, ' ', ui::App::COLS);
  line[ui::App::COLS] = 0;
  for (size_t i = 0; i < segLen; i++) {
    int col = x + (int)i;
    if (col >= 0 && col < ui::App::COLS) line[col] = seg[i];
  }
}

// Header: the open chat's name, and "#" at the right edge when any chat has unread messages.
template <typename Emit>
void forEachChatHeader(const ui::App& a, Emit emit) {
  if (a.msg.openSlot == comm::Messenger::NONE) return;
  char name[12];
  if (a.msg.openSlot == comm::ALL_ID) snprintf(name, sizeof name, "ALL");
  else snprintf(name, sizeof name, "CALC %u", (unsigned)a.msg.openSlot);
  char line[ui::App::COLS + 1];
  placeAt(line, 0, name, strlen(name));
  if (a.msg.hasUnread()) line[ui::App::COLS - 1] = '#';
  emit(line);
}

// The visible lines of the open chat, from the top of the view down.
// Your messages are on the left, other people's on the right, in bubbles.
// The typing line (plain text, left) is last.
template <typename Emit>
void forEachChatLine(const ui::App& a, Emit emit) {
  if (a.msg.openSlot == comm::Messenger::NONE) return;
  const comm::Chat& c = a.msg.chats[a.msg.openSlot];
  const uint16_t first = a.firstVisibleLine();
  uint16_t line = 0;      // which line of the whole chat we are on
  uint16_t shown = 0;     // how many lines we have emitted
  char out[ui::App::COLS + 1];

  auto show = [&](const char* text, size_t len, int x) {
    placeAt(out, x, text, len);
    if (line >= first && shown < ui::App::VIEW_LINES) { emit(out); shown++; }
    line++;
  };

  for (uint8_t i = 0; i < c.n; i++) {
    const comm::Msg& m = c.msgs[i];
    bool mine = m.from == DEVICE_ID;
    uint8_t w = m.len < ui::App::BUBBLE_TEXT ? m.len : ui::App::BUBBLE_TEXT;   // bubble inner width
    int boxW = w + 2;
    int x = mine ? 0 : ui::App::COLS - boxW;

    if (!mine) {                                   // sender's name above their bubble
      char name[12];
      snprintf(name, sizeof name, "CALC %u", (unsigned)m.from);
      show(name, strlen(name), ui::App::COLS - strlen(name));
    }
    char edge[ui::App::COLS + 1];
    edge[0] = '+';
    for (int k = 0; k < w; k++) edge[1 + k] = '-';
    edge[boxW - 1] = '+';
    show(edge, boxW, x);

    for (uint8_t start = 0; start < m.len; start += ui::App::BUBBLE_TEXT) {
      uint8_t n = (uint8_t)(m.len - start < ui::App::BUBBLE_TEXT ? m.len - start : ui::App::BUBBLE_TEXT);
      char row[ui::App::COLS + 1];
      row[0] = '|';
      memset(row + 1, ' ', w);
      char ascii[ui::App::BUBBLE_TEXT + 1];
      toAscii(ascii, m.text + start, n);
      memcpy(row + 1, ascii, n);
      row[boxW - 1] = '|';
      show(row, boxW, x);
    }

    show(edge, boxW, x);
  }

  // Empty chat with nothing typed: say so. This takes the place of the typing line.
  if (c.n == 0 && a.draft.len == 0) {
    const char* hint = "No messages";
    show(hint, strlen(hint), (ui::App::COLS - (int)strlen(hint)) / 2);
    return;
  }

  // Typing line: plain text on the left, wrapped at COLS.
  uint8_t parts = ui::App::linesFor(a.draft.len);
  for (uint8_t k = 0; k < parts; k++) {
    uint8_t start = (uint8_t)(k * ui::App::COLS);
    uint8_t n = (uint8_t)(a.draft.len > start ? (a.draft.len - start < ui::App::COLS ? a.draft.len - start : ui::App::COLS) : 0);
    char ascii[ui::App::COLS + 1];
    toAscii(ascii, a.draft.text + start, n);
    show(ascii, n, 0);
  }
}

// Calculator screen. The text uses the fewest rows that fit, and each row count has its own
// font size: 1 row fills the screen height, 2 rows are half that, 3 rows are a third.
// Characters per row are placeholders until the fonts are chosen and measured on the board.
//   font for 1 row: about 64 px tall (fills the screen)
//   font for 2 rows: about 32 px tall
//   font for 3 rows: about 20 px tall
struct CalcLevel {
  uint8_t rows;
  uint8_t colsPerRow;     // placeholder: characters that fit one row at this font size
  uint8_t fontHeight;     // pixels, for the drawing code
};
constexpr CalcLevel CALC_LEVELS[3] = {
  {1, 4, 64},
  {2, 8, 32},
  {3, 12, 20},
};
constexpr uint8_t CALC_MAX = CALC_LEVELS[2].rows * CALC_LEVELS[2].colsPerRow;   // most characters on the screen
constexpr uint8_t CALC_ERROR_LEN = 12;   // "Syntax error" fits one row at the smallest size

// Which size to use for this text: the fewest rows that hold it.
inline CalcLevel calcLevelFor(size_t n, bool isError) {
  if (isError) return CALC_LEVELS[2];
  for (uint8_t i = 0; i < 3; i++) {
    if (n <= (size_t)CALC_LEVELS[i].rows * CALC_LEVELS[i].colsPerRow) return CALC_LEVELS[i];
  }
  return CALC_LEVELS[2];
}

// Emits one line per row. The last row used is right-aligned, so the newest character sits
// at the right edge. Errors are emitted as one centred line.
template <typename Emit>
void forEachCalcLine(const char* text, bool isError, Emit emit) {
  size_t n = strlen(text);
  CalcLevel lvl = calcLevelFor(n, isError);
  const uint8_t cols = lvl.colsPerRow;
  if (isError) {
    char line[CALC_LEVELS[2].colsPerRow + 1];
    memset(line, ' ', cols);
    memcpy(line, text, n < cols ? n : cols);
    line[cols] = 0;
    emit(line);
    return;
  }
  uint8_t rows = n == 0 ? 1 : (uint8_t)((n + cols - 1) / cols);
  if (rows > lvl.rows) rows = lvl.rows;
  for (uint8_t r = 0; r < rows; r++) {
    size_t start = (size_t)r * cols;
    size_t len = n > start ? (n - start < cols ? n - start : cols) : 0;
    char line[CALC_LEVELS[2].colsPerRow + 1];
    if (r == rows - 1) {                       // last row: right-aligned
      memset(line, ' ', cols);
      memcpy(line + (cols - len), text + start, len);
    } else {                                   // full row
      memcpy(line, text + start, len);
      memset(line + len, ' ', cols - len);
    }
    line[cols] = 0;
    emit(line);
  }
}

}  // namespace screen
