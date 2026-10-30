// Screen logic: which screen is showing, and what each key does on it.
// Calculator keys are handed back to the calculator code (main.cpp).
// No hardware code here, so it can be tested on a computer.
#pragma once
#include <stdint.h>
#include "comm.h"

namespace ui {

// Keys as the UI sees them (main.cpp maps its keypad to these).
enum class Key : uint8_t {
  NONE,
  K0, K1, K2, K3, K4, K5, K6, K7, K8, K9, DOT,
  ADD, SUB, MUL, DIV, POW, SQ, SQRT, EQ, LP, RP,
  C, DEL, MEMW, MR, MC, COMP, FD, COMMA, SIGN
};

enum class Screen : uint8_t { CALC, CONTACTS, CHAT };

// What happened to a key press.
enum class Result : uint8_t {
  IGNORED,          // this screen doesn't use the key
  HANDLED,          // screen changed or text was typed
  SEND,             // a message was sent; see App::outbox
  FORWARD_TO_CALC   // calculator screen: main.cpp should run the key through the calculator
};

class App {
 public:
  static constexpr uint8_t NUM_CONTACTS = 4;   // ALL, then the three other calculators
  static constexpr uint8_t COLS = 21;          // characters per line; set when the screen is done
  static constexpr uint8_t VIEW_LINES = 6;     // lines visible in a chat; set when the screen is done

  uint8_t scroll = 0;               // lines scrolled up from the bottom of the open chat

  Screen screen = Screen::CALC;
  uint8_t sel = 0;                  // highlighted row on the contacts screen, 0..4
  comm::Messenger msg;
  comm::Composer draft;             // the message being typed in the open chat
  comm::Packet outbox;              // filled when Result::SEND is returned

  // Contacts list row -> chat slot. Row 0 is ALL; then the other boards in number order,
  // skipping this board (DEVICE_ID).
  static uint8_t slotForRow(uint8_t row) {
    if (row == 0) return comm::ALL_ID;
    uint8_t n = 0;
    for (uint8_t id = 1; id <= 4; id++) {
      if (id == DEVICE_ID) continue;
      if (++n == row) return id;
    }
    return comm::ALL_ID;
  }

  static uint8_t linesFor(uint8_t len) { return len == 0 ? 1 : (uint8_t)((len + COLS - 1) / COLS); }

  // Bubbles: the text inside a bubble wraps at BUBBLE_TEXT characters, and the bubble is only
  // as wide as its longest line, so short messages get small bubbles.
  static constexpr uint8_t BUBBLE_TEXT = 14;

  static uint8_t bubbleTextLines(uint8_t len) {
    return len == 0 ? 1 : (uint8_t)((len + BUBBLE_TEXT - 1) / BUBBLE_TEXT);
  }

  // Lines one message takes: a name line for incoming messages, a top and bottom edge,
  // and one line per line of text.
  static uint8_t messageLines(const comm::Msg& m) {
    return (uint8_t)((m.from != DEVICE_ID ? 1 : 0) + 2 + bubbleTextLines(m.len));
  }

  // Chat slot -> contacts list row.
  static uint8_t rowForSlot(uint8_t slot) {
    if (slot == comm::ALL_ID) return 0;
    uint8_t n = 0;
    for (uint8_t id = 1; id <= 4; id++) {
      if (id == DEVICE_ID) continue;
      n++;
      if (id == slot) return n;
    }
    return 0;
  }

  // Handle a key press. Returns what the caller should do next.
  Result handle(Key k, uint32_t now) {
    switch (screen) {
      case Screen::CALC:     return onCalc(k);
      case Screen::CONTACTS: return onContacts(k);
      case Screen::CHAT:     return onChat(k, now);
    }
    return Result::IGNORED;
  }

  // An incoming packet from the radio. Returns true if it was stored.
  bool onReceive(const comm::Packet& p, uint32_t now) {
    bool ok = msg.receive(p, now);
    if (ok && screen == Screen::CHAT) scroll = 0;   // show the newest message
    return ok;
  }

  // Lines in the open chat: every message, then the message being typed.
  uint16_t chatLines() const {
    if (msg.openSlot == comm::Messenger::NONE) return 0;
    const comm::Chat& c = msg.chats[msg.openSlot];
    uint16_t total = 0;
    for (uint8_t i = 0; i < c.n; i++) total += messageLines(c.msgs[i]);
    return total + linesFor(draft.len);
  }

  // The first line the screen shows, counted from the top of the chat.
  uint16_t firstVisibleLine() const {
    uint16_t total = chatLines();
    uint16_t top = total > VIEW_LINES ? total - VIEW_LINES : 0;
    return scroll >= top ? 0 : top - scroll;
  }

  // What the contacts screen needs to draw: a row shows an unread square when true.
  bool rowUnread(uint8_t row) const { return msg.chats[slotForRow(row)].unread; }

 private:
  Result onCalc(Key k) {
    if (k == Key::COMMA) { screen = Screen::CONTACTS; return Result::HANDLED; }
    return Result::FORWARD_TO_CALC;                       // includes COMM B (+/-)
  }

  Result onContacts(Key k) {
    switch (k) {
      case Key::COMMA:
        screen = Screen::CALC;
        return Result::HANDLED;
      case Key::RP:                    // ) moves the selection down
        sel = (sel + 1) % NUM_CONTACTS;
        return Result::HANDLED;
      case Key::LP:                    // ( moves the selection up
        sel = (sel + NUM_CONTACTS - 1) % NUM_CONTACTS;
        return Result::HANDLED;
      case Key::EQ:
        msg.open(slotForRow(sel));
        draft.clear();                // assumption: a draft is not kept between chats
        screen = Screen::CHAT;
        return Result::HANDLED;
      default:
        return Result::IGNORED;
    }
  }

  Result onChat(Key k, uint32_t now) {
    // < and > scroll the chat. Any other key brings the view back to the bottom.
    if (k == Key::LP) {
      uint16_t total = chatLines();
      uint8_t maxScroll = total > VIEW_LINES ? total - VIEW_LINES : 0;
      if (scroll < maxScroll) scroll++;
      return Result::HANDLED;
    }
    if (k == Key::RP) {
      if (scroll > 0) scroll--;
      return Result::HANDLED;
    }
    if (k != Key::COMMA) scroll = 0;

    switch (k) {
      case Key::COMMA:                 // leave the chat, back to the calculator
        sel = rowForSlot(msg.openSlot == comm::Messenger::NONE ? 0 : msg.openSlot);
        msg.close();
        screen = Screen::CALC;
        return Result::HANDLED;

      case Key::K0: case Key::K1: case Key::K2: case Key::K3: case Key::K4:
      case Key::K5: case Key::K6: case Key::K7: case Key::K8: case Key::K9:
        draft.digit(digitChar(k), now);
        return Result::HANDLED;

      case Key::DOT:   draft.symbol('.'); return Result::HANDLED;
      case Key::ADD:   draft.symbol('+'); return Result::HANDLED;
      case Key::SUB:   draft.symbol(comm::SYM_SUB); return Result::HANDLED;
      case Key::MUL:   draft.symbol(comm::SYM_MUL); return Result::HANDLED;
      case Key::DIV:   draft.symbol(comm::SYM_DIV); return Result::HANDLED;
      case Key::POW:   draft.symbol(comm::SYM_POW); return Result::HANDLED;
      case Key::SQ:    draft.replaceLastNumber(true); return Result::HANDLED;
      case Key::SQRT:  draft.replaceLastNumber(false); return Result::HANDLED;

      case Key::C:     draft.clear(); return Result::HANDLED;
      case Key::DEL:   draft.backspace(); return Result::HANDLED;

      case Key::EQ:
        if (draft.equals(now) && msg.send(msg.openSlot, draft.text, draft.len, now, &outbox)) {
          draft.clear();
          return Result::SEND;
        }
        return Result::HANDLED;

      case Key::FD:    draft.toggleFraction(); return Result::HANDLED;
      case Key::COMP:  draft.startComp(); return Result::HANDLED;

      // Memory keys are not used in messages.
      case Key::MEMW: case Key::MR: case Key::MC:
        return Result::IGNORED;

      default:
        return Result::IGNORED;
    }
  }

  static char digitChar(Key k) { return (char)('0' + ((int)k - (int)Key::K0)); }
};

}  // namespace ui
