#include "test_util.h"
#include <stdio.h>
using namespace comm;

static int fails = 0, passes = 0;
#define CHECK(cond, what) do { if (cond) passes++; else { fails++; printf("FAIL: %s\n", what); } } while (0)

static void typeDigits(Composer& c, const char* keys, uint32_t& t, uint32_t gap) {
  for (const char* p = keys; *p; p++) { c.digit(*p, t); t += gap; }
}

int main() {
  // Multi-tap: 2 pressed quickly cycles 2, A, B, C, 2
  {
    Composer c; uint32_t t = 0;
    c.digit('2', t); t += 100; CHECK(!strcmp(c.text, "2"), "tap 1 gives 2");
    c.digit('2', t); t += 100; CHECK(!strcmp(c.text, "A"), "tap 2 gives A");
    c.digit('2', t); t += 100; CHECK(!strcmp(c.text, "B"), "tap 3 gives B");
    c.digit('2', t); t += 100; CHECK(!strcmp(c.text, "C"), "tap 4 gives C");
    c.digit('2', t); t += 100; CHECK(!strcmp(c.text, "2"), "tap 5 wraps to 2");
  }
  // Waiting over 1 s starts a new letter: "22" typed as two characters
  {
    Composer c; uint32_t t = 0;
    c.digit('2', t); t += 1500;
    c.digit('2', t);
    CHECK(!strcmp(c.text, "22"), "pause starts new char");
  }
  // Different key starts a new char: "21"
  {
    Composer c; uint32_t t = 0;
    c.digit('2', t); t += 100; c.digit('1', t);
    CHECK(!strcmp(c.text, "21"), "different key starts new char");
  }
  // "1" cycles through . , ? !
  {
    Composer c; uint32_t t = 0;
    c.digit('1', t); t += 100; c.digit('1', t); t += 100; c.digit('1', t);
    CHECK(!strcmp(c.text, ","), "1 pressed three times gives ,");
  }
  // 0 cycles space
  {
    Composer c; uint32_t t = 0;
    c.digit('0', t); t += 100; c.digit('0', t);
    CHECK(!strcmp(c.text, " "), "0 pressed twice gives space");
  }
  // Symbols type straight away and break a multi-tap run
  {
    Composer c; uint32_t t = 0;
    c.digit('2', t); c.symbol('+'); t += 100; c.digit('2', t);
    CHECK(!strcmp(c.text, "2+2"), "symbol ends multi-tap");
    c.symbol(SYM_MUL); c.symbol(SYM_POW); c.symbol(SYM_DIV);
    CHECK(c.len == 6, "unicode-ish symbols each one char");
  }
  // Double "=" sends and removes the typed "="
  {
    Composer c; uint32_t t = 1000;
    c.digit('5', t);
    bool s1 = c.equals(t + 100);
    CHECK(!s1 && !strcmp(c.text, "5="), "first = types =");
    bool s2 = c.equals(t + 300);
    CHECK(s2 && !strcmp(c.text, "5"), "second = within 0.5 s sends and removes =");
  }
  // Slow "=" presses do not send
  {
    Composer c; uint32_t t = 1000;
    c.digit('5', t);
    c.equals(t + 100);
    bool s2 = c.equals(t + 900);
    CHECK(!s2 && !strcmp(c.text, "5=="), "slow second = does not send");
  }
  // Empty message: double "=" does not send
  {
    Composer c; uint32_t t = 0;
    c.equals(t); t += 100;
    bool s = c.equals(t);
    CHECK(!s, "empty message does not send");
  }
  // DEL removes last char, C clears
  {
    Composer c; uint32_t t = 0;
    typeDigits(c, "123", t, 2000);
    c.backspace();
    CHECK(!strcmp(c.text, "12"), "DEL removes last char");
    c.clear();
    CHECK(c.len == 0 && c.text[0] == 0, "C clears message");
  }
  // x2 and sqrt act on the number being typed
  {
    Composer c; uint32_t t = 0;
    c.digit('2', t); c.symbol('+'); c.digit('2', t);
    CHECK(c.replaceLastNumber(true) && !strcmp(c.text, "2+4"), "square last number");
    CHECK(c.replaceLastNumber(false) && !strcmp(c.text, "2+2"), "sqrt last number");
    Composer d; d.symbol('+');
    CHECK(!d.replaceLastNumber(true), "no number: nothing changes");
    Composer e; e.digit('2', 0); e.symbol('.'); e.digit('5', 5000);
    CHECK(e.replaceLastNumber(true) && !strcmp(e.text, "6.25"), "square 2.5 gives 6.25");
  }
  // F<->D on the typed number
  {
    Composer c; uint32_t t = 0;
    c.digit('2', t); c.symbol('.'); c.digit('5', t + 100);
    CHECK(c.toggleFraction() && !strcmp(c.text, "5/2"), "F<->D: 2.5 becomes 5/2");
    CHECK(c.toggleFraction() && !strcmp(c.text, "2.5"), "F<->D back: 5/2 becomes 2.5");
  }
  // F<->D on whole numbers and on text with no number
  {
    Composer c; uint32_t t = 0;
    c.digit('4', t);
    CHECK(c.toggleFraction() && !strcmp(c.text, "4/1"), "F<->D: 4 becomes 4/1");
    Composer d; d.symbol('+');
    CHECK(!d.toggleFraction(), "F<->D with no number does nothing");
  }
  // COMP on a typed fraction: 5/2 COMP 3 = gives 15/6 (same as the calculator)
  {
    Composer c; uint32_t t = 0;
    c.digit('2', t); c.symbol('.'); c.digit('5', t + 100);
    c.toggleFraction();
    CHECK(c.startComp() && c.compPending(), "COMP starts");
    c.digit('3', t + 200);
    CHECK(!c.equals(t + 300), "COMP = does not send");
    CHECK(!strcmp(c.text, "15/6") && !c.compPending(), "COMP 3 = gives 15/6");
  }
  // COMP on a decimal: 2.5 is reduced to 5/2 first, then COMP 2 = gives 10/4
  {
    Composer c; uint32_t t = 0;
    c.digit('2', t); c.symbol('.'); c.digit('5', t + 100);
    c.startComp(); c.digit('2', t + 200); c.equals(t + 300);
    CHECK(!strcmp(c.text, "10/4"), "COMP on decimal 2.5 x 2 gives 10/4");
  }
  // While COMP waits, digits do not go into the message text
  {
    Composer c; uint32_t t = 0;
    c.digit('3', t); c.symbol('/'); c.digit('4', t);
    c.startComp();
    c.digit('9', t);
    CHECK(!strcmp(c.text, "3/4"), "COMP digits not added to text");
  }
  // Pressing another key cancels COMP
  {
    Composer c; uint32_t t = 0;
    c.digit('3', t); c.symbol('/'); c.digit('4', t);
    c.startComp();
    c.symbol('+');
    CHECK(!c.compPending() && !strcmp(c.text, "3/4+"), "other key cancels COMP");
  }
  // "=" with nothing typed after COMP cancels it and does not send
  {
    Composer c; uint32_t t = 0;
    c.digit('3', t); c.symbol('/'); c.digit('4', t);
    c.startComp();
    CHECK(!c.equals(t + 10) && !c.compPending() && !strcmp(c.text, "3/4"), "COMP with no number cancels");
  }
  // x2 and sqrt on fractions
  {
    Composer c; uint32_t t = 0;
    c.digit('5', t); c.symbol('/'); c.digit('2', t);
    CHECK(c.replaceLastNumber(true) && !strcmp(c.text, "25/4"), "5/2 squared is 25/4");
    CHECK(c.replaceLastNumber(false) && !strcmp(c.text, "5/2"), "sqrt of 25/4 is 5/2");
    Composer d; d.digit('2', t); d.symbol('/'); d.digit('3', t);
    CHECK(d.replaceLastNumber(false) && !strcmp(d.text, "0.8164965809"), "sqrt of 2/3 is decimal");
  }
  // Power symbol goes in the message as one character
  {
    Composer c; uint32_t t = 0;
    c.digit('2', t); c.symbol(comm::SYM_POW); c.digit('3', t + 100);
    CHECK(c.len == 3 && c.text[1] == comm::SYM_POW, "power symbol is one character");
  }
  // Message length cap at 120
  {
    Composer c; uint32_t t = 0;
    for (int i = 0; i < 130; i++) c.symbol('x');
    CHECK(c.len == MAX_MSG_LEN, "message capped at 120");
  }

  // ---- messaging ----
  // Incoming direct message goes to sender's chat and sets unread
  {
    Messenger m; Packet p{}; p.from = peer(0); p.to = DEVICE_ID; p.len = 2; memcpy(p.text, "hi", 3);
    bool ok = m.receive(p, 1000);
    CHECK(ok && m.chats[peer(0)].n == 1 && m.chats[peer(0)].unread, "direct message stored and unread");
    CHECK(m.hasUnread(), "hasUnread true");
    m.open(peer(0));
    CHECK(!m.chats[peer(0)].unread, "opening clears unread");
    CHECK(!m.hasUnread(), "hasUnread false after open");
  }
  // Message to ALL goes to the ALL chat
  {
    Messenger m; Packet p{}; p.from = peer(1); p.to = ALL_ID; p.len = 3; memcpy(p.text, "yo!", 4);
    m.receive(p, 1000);
    CHECK(m.chats[ALL_ID].n == 1 && m.chats[ALL_ID].unread, "ALL message in ALL chat");
    CHECK(m.chats[peer(1)].n == 0, "ALL message not in sender's direct chat");
  }
  // Message for someone else is ignored
  {
    Messenger m; Packet p{}; p.from = peer(0); p.to = peer(1); p.len = 1; memcpy(p.text, "x", 2);
    CHECK(!m.receive(p, 1000) && m.chats[peer(0)].n == 0, "message for another board ignored");
  }
  // Own echo / bad sender ignored
  {
    Messenger m; Packet p{}; p.from = DEVICE_ID; p.to = ALL_ID; p.len = 1; memcpy(p.text, "x", 2);
    CHECK(!m.receive(p, 1000), "own packet ignored");
    p.from = 0;
    CHECK(!m.receive(p, 1000), "sender 0 ignored");
  }
  // Sending: stores in own chat and fills packet
  {
    Messenger m; Packet out{};
    bool ok = m.send(peer(1), "hey", 3, 1000, &out);
    CHECK(ok && out.from == DEVICE_ID && out.to == peer(1) && !strcmp(out.text, "hey"), "send fills packet");
    CHECK(m.chats[peer(1)].n == 1 && m.chats[peer(1)].msgs[0].from == DEVICE_ID, "sent message stored as mine");
    CHECK(!m.send(DEVICE_ID, "x", 1, 1000, &out), "cannot send to self");
    CHECK(!m.send(ALL_ID, "", 0, 1000, &out), "empty send rejected");
  }
  // Messages older than 60 min are dropped
  {
    Messenger m; Packet p{}; p.from = peer(0); p.to = DEVICE_ID; p.len = 1; memcpy(p.text, "a", 2);
    m.receive(p, 0);
    p.len = 1; memcpy(p.text, "b", 2);
    uint32_t later = MSG_TTL_MS + 5000;
    m.receive(p, later);
    CHECK(m.chats[peer(0)].n == 1 && m.chats[peer(0)].msgs[0].text[0] == 'b', "old message dropped after 60 min");
  }
  // Only 30 per chat: oldest dropped
  {
    Messenger m; Packet p{}; p.from = peer(2); p.to = DEVICE_ID; p.len = 1;
    for (int i = 0; i < 35; i++) { p.text[0] = (char)('a' + i % 26); p.text[1] = 0; m.receive(p, 1000 + i); }
    CHECK(m.chats[peer(2)].n == MAX_CHAT_MSGS, "chat holds at most 30");
    CHECK(m.chats[peer(2)].msgs[0].text[0] == (char)('a' + 5), "oldest 5 dropped");
  }

  printf("\n%d passed, %d failed\n", passes, fails);
  return fails ? 1 : 0;
}
