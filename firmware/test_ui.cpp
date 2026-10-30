#include "ui.h"
#include <stdio.h>
using namespace ui;

static int fails = 0, passes = 0;
#define CHECK(cond, what) do { if (cond) passes++; else { fails++; printf("FAIL: %s\n", what); } } while (0)

static comm::Packet pkt(uint8_t from, uint8_t to, const char* text) {
  comm::Packet p{};
  p.from = from; p.to = to; p.len = (uint8_t)strlen(text);
  memcpy(p.text, text, p.len + 1);
  return p;
}

int main() {
  // Calculator: keys go to the calculator, COMM A opens contacts
  {
    App a;
    CHECK(a.handle(Key::K2, 0) == Result::FORWARD_TO_CALC, "calc: digits forwarded");
    CHECK(a.handle(Key::COMMA, 0) == Result::HANDLED && a.screen == Screen::CONTACTS, "COMM A opens contacts");
  }
  // Contacts: ) and ( move the selection, wrap, = opens chat; + and - do nothing here
  {
    App a;
    a.handle(Key::COMMA, 0);
    a.handle(Key::RP, 0); CHECK(a.sel == 1, ") moves down to CALC 2");
    a.handle(Key::LP, 0); a.handle(Key::LP, 0); CHECK(a.sel == 3, "( from the top wraps to the last row");
    a.handle(Key::RP, 0); CHECK(a.sel == 0, ") from the last row wraps to ALL");
    CHECK(a.handle(Key::ADD, 0) == Result::IGNORED && a.sel == 0, "+ does nothing on the contacts list");
    CHECK(a.handle(Key::SUB, 0) == Result::IGNORED && a.sel == 0, "- does nothing on the contacts list");
    a.sel = 0;
    CHECK(a.handle(Key::EQ, 0) == Result::HANDLED && a.screen == Screen::CHAT, "= on ALL opens chat");
    CHECK(a.msg.openSlot == comm::ALL_ID, "open chat is ALL");
  }
  // Contacts row to chat slot mapping
  {
    // board 1: ALL, then CALC 2, CALC 3, CALC 4
    CHECK(App::slotForRow(0) == 0 && App::slotForRow(1) == 2 && App::slotForRow(3) == 4, "row to slot");
    CHECK(App::rowForSlot(0) == 0 && App::rowForSlot(2) == 1 && App::rowForSlot(4) == 3, "slot to row");
  }
  // Unread square shows on contacts, clears when chat opened
  {
    App a;
    a.onReceive(pkt(3, DEVICE_ID, "hi"), 1000);
    CHECK(a.rowUnread(App::rowForSlot(3)), "unread on CALC 3 row");
    CHECK(!a.rowUnread(App::rowForSlot(2)), "no unread on CALC 2 row");
    a.handle(Key::COMMA, 0);
    a.sel = App::rowForSlot(3);
    a.handle(Key::EQ, 0);
    CHECK(!a.rowUnread(App::rowForSlot(3)), "opening clears unread");
  }
  // Incoming to ALL while on calc screen: unread on ALL row
  {
    App a;
    a.onReceive(pkt(2, comm::ALL_ID, "yo"), 1000);
    CHECK(a.rowUnread(0), "unread on ALL row");
  }
  // Chat: type, send with double =, message appears in history, packet produced
  {
    App a;
    a.handle(Key::COMMA, 0);
    a.sel = App::rowForSlot(2);
    a.handle(Key::EQ, 0);                         // open CALC 2
    uint32_t t = 10000;
    a.handle(Key::K2, t); a.handle(Key::K2, t + 100);   // "A"
    a.handle(Key::K5, t + 2000);                  // "5"
    CHECK(!strcmp(a.draft.text, "A5"), "typed A5");
    CHECK(a.handle(Key::EQ, t + 2100) == Result::HANDLED, "first = types =");
    Result r = a.handle(Key::EQ, t + 2200);
    CHECK(r == Result::SEND, "double = sends");
    CHECK(a.outbox.to == 2 && a.outbox.from == DEVICE_ID && !strcmp(a.outbox.text, "A5"), "packet to CALC 2");
    CHECK(a.msg.chats[2].n == 1 && !strcmp(a.msg.chats[2].msgs[0].text, "A5"), "sent message in history");
    CHECK(a.draft.len == 0, "draft cleared after send");
  }
  // Chat: symbols, x2 and sqrt type the special symbols, DEL and C
  {
    App a;
    a.handle(Key::COMMA, 0); a.handle(Key::EQ, 0);   // open ALL
    a.handle(Key::K3, 0); a.handle(Key::ADD, 0); a.handle(Key::K2, 0);
    a.handle(Key::SQ, 0);
    CHECK(!strcmp(a.draft.text, "3+4"), "x2 squares the typed number in a message");
    a.handle(Key::SQRT, 0);
    CHECK(!strcmp(a.draft.text, "3+2"), "sqrt takes the root of the typed number");
    a.handle(Key::DEL, 0);
    CHECK(a.draft.len == 2, "DEL removes one");
    a.handle(Key::C, 0);
    CHECK(a.draft.len == 0, "C clears");
  }
  // Chat: COMM A goes to the calculator; COMM A twice more gets to the contacts list
  {
    App a;
    a.handle(Key::COMMA, 0); a.handle(Key::EQ, 0);
    CHECK(a.screen == Screen::CHAT, "in chat");
    a.handle(Key::COMMA, 0);
    CHECK(a.screen == Screen::CALC && a.msg.openSlot == comm::Messenger::NONE, "COMM A leaves chat to calculator");
    a.handle(Key::COMMA, 0);
    CHECK(a.screen == Screen::CONTACTS, "COMM A from calculator opens contacts");
  }
  // COMM B: +/- on the calculator, ignored in a message
  {
    App a;
    CHECK(a.handle(Key::SIGN, 0) == Result::FORWARD_TO_CALC, "sign key goes to calculator");
    a.handle(Key::COMMA, 0); a.handle(Key::EQ, 0);
    CHECK(a.handle(Key::SIGN, 0) == Result::IGNORED, "sign key ignored in a message");
  }
  // Chat: calculator-only keys ignored
  {
    App a;
    a.handle(Key::COMMA, 0); a.handle(Key::EQ, 0);
    CHECK(a.handle(Key::COMP, 0) == Result::HANDLED, "COMP works in chat");
    CHECK(a.handle(Key::MR, 0) == Result::IGNORED, "MR ignored in chat");
  }
  // F<->D and COMP work in a message
  {
    App a;
    a.handle(Key::COMMA, 0); a.handle(Key::EQ, 0);
    a.handle(Key::K2, 0); a.handle(Key::DOT, 0); a.handle(Key::K5, 0);
    a.handle(Key::FD, 0);
    CHECK(!strcmp(a.draft.text, "5/2"), "FD in message makes 5/2");
    a.handle(Key::COMP, 0); a.handle(Key::K3, 0);
    CHECK(a.handle(Key::EQ, 0) == Result::HANDLED, "COMP = in message does not send");
    CHECK(!strcmp(a.draft.text, "15/6") && a.msg.chats[0].n == 0, "COMP 3 in message gives 15/6, nothing sent");
  }
  // Power key types the power symbol in messages
  {
    App a;
    a.handle(Key::COMMA, 0); a.handle(Key::EQ, 0);
    a.handle(Key::K2, 0); a.handle(Key::POW, 0);
    CHECK(a.draft.text[1] == comm::SYM_POW, "POW types power symbol");
  }
  // < and > scroll a long chat; typing brings the view back to the bottom
  {
    App a;
    a.handle(Key::COMMA, 0);                           // contacts
    a.sel = 0; a.handle(Key::EQ, 0);                   // open ALL
    comm::Packet p{}; p.from = 2; p.to = comm::ALL_ID; p.len = 40; memset(p.text, 'x', 40); p.text[40] = 0;
    for (int i = 0; i < 10; i++) a.onReceive(p, 1000 + i);      // 10 messages x 2 lines = 20 lines
    // each incoming bubble is 6 lines (name, top, 3 text lines, bottom); 60 + typing line = 61
    for (int i = 0; i < 60; i++) a.handle(Key::LP, 0);
    CHECK(a.scroll == 55, "scroll stops at top (61 lines - 6 visible)");
    a.handle(Key::RP, 0); a.handle(Key::RP, 0); a.handle(Key::RP, 0);
    CHECK(a.scroll == 52, "> scrolls back down");
    a.handle(Key::K3, 0);
    CHECK(a.scroll == 0, "typing returns to the bottom");
    CHECK(a.firstVisibleLine() == 55, "bottom view starts at line 55 (61 lines total)");
  }
  // Memory keys still ignored in messages
  {
    App a;
    a.handle(Key::COMMA, 0); a.handle(Key::EQ, 0);
    CHECK(a.handle(Key::MEMW, 0) == Result::IGNORED && a.handle(Key::MR, 0) == Result::IGNORED, "memory keys ignored in message");
  }
  // Receiving while chat is open: stored, no unread
  {
    App a;
    a.handle(Key::COMMA, 0); a.sel = App::rowForSlot(2); a.handle(Key::EQ, 0);
    a.onReceive(pkt(2, DEVICE_ID, "ok"), 500);
    CHECK(!a.rowUnread(1) && a.msg.chats[2].n == 1, "message arrives in open chat, no unread");
  }
  printf("%d passed, %d failed\n", passes, fails);
  return fails ? 1 : 0;
}
