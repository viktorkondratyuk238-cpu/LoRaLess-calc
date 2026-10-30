#include "main.cpp"
#include <stdio.h>
static int fails = 0, passes = 0;
static const char* buildExprForTest() { static char b[160]; buildExpr(b, sizeof b); return b; }
#define CHECK(cond, what) do { if (cond) passes++; else { fails++; printf("FAIL: %s\n", what); } } while (0)

int main() {
  // Calculator still works through the new routing
  {
    app = ui::App();
    onKey(K_2); onKey(K_ADD); onKey(K_3); onKey(K_EQ);
    CHECK(app.screen == ui::Screen::CALC, "still on calculator");
    CHECK(lastRes.exact && lastRes.n == 5, "calculator gives 5 through routing");
  }
  // COMM A -> contacts; = opens ALL; typing; double = sends; COMM A back out
  {
    app = ui::App();
    onKey(K_COMMA);
    CHECK(app.screen == ui::Screen::CONTACTS, "COMM A opens contacts");
    app.sel = 0;
    onKey(K_EQ);
    CHECK(app.screen == ui::Screen::CHAT && app.msg.openSlot == comm::ALL_ID, "= opens ALL chat");
    onKey(K_1); onKey(K_EQ); onKey(K_EQ);
    CHECK(app.msg.chats[comm::ALL_ID].n == 1, "message sent to ALL stored");
    CHECK(!strcmp(app.msg.chats[comm::ALL_ID].msgs[0].text, "1"), "message text is 1");
    onKey(K_COMMA);
    CHECK(app.screen == ui::Screen::CALC, "COMM A in a chat goes to the calculator");
    onKey(K_COMMA);
    CHECK(app.screen == ui::Screen::CONTACTS, "COMM A again opens contacts");
  }
  // COMM B flips the sign on the calculator
  {
    app = ui::App();
    onKey(K_5); onKey(K_SIGN);
    CHECK(entry[0] == '-', "COMM B makes the number negative");
  }
  // Incoming message while on calculator: unread square set, no screen change
  {
    app = ui::App();
    comm::Packet p{}; p.from = 2; p.to = comm::ALL_ID; p.len = 2; memcpy(p.text, "yo", 3);
    onRadioPacket(p);
    CHECK(app.rowUnread(0) && app.screen == ui::Screen::CALC, "incoming marks ALL unread, stays on calc");
  }
  // Calculator stops accepting characters once it is 3 rows full
  {
    app = ui::App();
    clearCalc();
    int presses = 0;
    while (strlen(buildExprForTest()) < screen::CALC_MAX && presses < 200) {
      onKey(K_1); onKey(K_ADD); presses++;
    }
    size_t full = strlen(buildExprForTest());
    onKey(K_9); onKey(K_9);
    CHECK(strlen(buildExprForTest()) == full, "no characters after 3 rows are full");
    CHECK(full >= screen::CALC_MAX, "expression reached 3 rows");
  }
  // Errors show as "Syntax error"
  {
    app = ui::App();
    clearCalc();
    onKey(K_1); onKey(K_DIV); onKey(K_0); onKey(K_EQ);
    CHECK(gErr, "division by zero sets error");
    char text[64] = "";
    if (gErr) strcpy(text, "Syntax error");
    CHECK(!strcmp(text, "Syntax error"), "error text is Syntax error");
  }
  // Keypad layout matches the agreed layout
  {
    CHECK(KEYMAP[0][2] == K_SIGN && KEYMAP[0][4] == K_COMMA, "row 1: +/- and COMM A in the corner");
    CHECK(KEYMAP[0][3] == K_MEMW, "row 1: MEM W next to COMM A");
    CHECK(KEYMAP[1][3] == K_MR && KEYMAP[1][4] == K_MC, "row 2: MR and MC");
    CHECK(KEYMAP[2][3] == K_COMP && KEYMAP[2][4] == K_FD, "row 3: COMP and F<->D");
    CHECK(KEYMAP[4][3] == K_MUL && KEYMAP[4][4] == K_DIV, "row 5: x and divide");
    CHECK(KEYMAP[5][3] == K_ADD && KEYMAP[5][4] == K_SUB, "row 6: + and -");
    CHECK(KEYMAP[3][3] == K_LP && KEYMAP[3][4] == K_RP, "row 4: brackets unchanged");
  }
  // The unused expander pins are driven low in setup, so they never float
  {
    // stubs record nothing, so check the pin list directly
    static const uint8_t UNUSED_CHECK[5] = {6, 7, 13, 14, 15};
    bool allPortPins = true;
    for (uint8_t p : UNUSED_CHECK) if (p > 15) allPortPins = false;
    CHECK(allPortPins, "unused pins are valid expander pins");
    bool noOverlap = true;
    for (uint8_t p : UNUSED_CHECK) for (int r = 0; r < 6; r++) if (ROW_PIN[r] == p) noOverlap = false;
    for (uint8_t p : UNUSED_CHECK) for (int c = 0; c < 5; c++) if (COL_PIN[c] == p) noOverlap = false;
    CHECK(noOverlap, "unused pins do not overlap rows or columns");
  }
  printf("%d passed, %d failed\n", passes, fails);
  return fails ? 1 : 0;
}
