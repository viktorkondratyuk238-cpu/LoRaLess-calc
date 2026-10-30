#include <vector>
#include <string>
#include "screen_text.h"
#include "test_util.h"
#include <stdio.h>
using namespace ui;

static int fails = 0, passes = 0;
#define CHECK(cond, what) do { if (cond) passes++; else { fails++; printf("FAIL: %s\n", what); } } while (0)

static std::vector<std::string> lines;
static void grab(const char* s) { lines.push_back(s); }
static std::string pad(const std::string& s) { return s + std::string(21 - s.size(), ' '); }

static comm::Packet pkt(uint8_t from, const std::string& text, uint8_t to = DEVICE_ID) {
  comm::Packet p{};
  p.from = from; p.to = to; p.len = (uint8_t)text.size();
  memcpy(p.text, text.c_str(), text.size() + 1);
  return p;
}

int main() {
  // Contacts: selected row marked, unread marked
  {
    App a;
    a.onReceive(pkt(peer(0), "hi"), 1000);
    a.sel = 1;
    lines.clear();
    screen::forEachContactLine(a, grab);
    CHECK(lines.size() == 4, "four contact rows: ALL and three other boards");
    CHECK(lines[1][0] == '*' && lines[0][0] == ' ', "selected row marked");
    CHECK(lines[1].find('#') != std::string::npos, "unread marked on the first other board");
    CHECK(lines[0].find('#') == std::string::npos, "no unread on ALL");
  }
  // Bubbles: your message on the left, theirs on the right with their name above
  {
    App a;
    a.handle(Key::COMMA, 0); a.sel = 0; a.handle(Key::EQ, 0);      // open ALL
    a.msg.send(comm::ALL_ID, "yo", 2, 100, &a.outbox);              // your message
    a.onReceive(pkt(peer(1), "hi", comm::ALL_ID), 200);             // another board to ALL
    for (int i = 0; i < 10; i++) a.handle(Key::LP, 0);              // scroll to the top
    lines.clear();
    screen::forEachChatLine(a, grab);
    // Your bubble (3 lines), then the sender's name and bubble (4 lines). Six lines fit on screen.
    std::string name = "CALC " + std::to_string(peer(1));
    CHECK(lines.size() == 6, "six lines shown at the top of the chat");
    CHECK(lines[0] == pad("+--+"), "your bubble on the left");
    CHECK(lines[1] == pad("|yo|"), "your text inside the bubble");
    CHECK(lines[3] == pad(std::string(21 - name.size(), ' ') + name), "sender name on the right");
    CHECK(lines[4] == pad(std::string(17, ' ') + "+--+"), "their bubble on the right");
    CHECK(lines[5] == pad(std::string(17, ' ') + "|hi|"), "their text inside the bubble");
    // Back to the bottom: the typing line is last
    for (int i = 0; i < 10; i++) a.handle(Key::RP, 0);
    lines.clear();
    screen::forEachChatLine(a, grab);
    CHECK(lines.back() == pad(""), "empty typing line at the bottom");
  }
  // Bubble width follows the message length (up to 14 characters per line)
  {
    App a;
    a.handle(Key::COMMA, 0); a.sel = 0; a.handle(Key::EQ, 0);
    a.msg.send(comm::ALL_ID, "abcdefghijklmnopqrstuvwxyz", 26, 100, &a.outbox);   // 26 characters
    lines.clear();
    screen::forEachChatLine(a, grab);
    CHECK(lines.size() == 5, "26 characters: top, two text lines, bottom, plus typing line");
    CHECK(lines[0] == pad("+--------------+"), "bubble width is capped at 14");
    CHECK(lines[1] == pad("|abcdefghijklmn|"), "first 14 characters on the first line");
    CHECK(lines[2] == pad("|opqrstuvwxyz  |"), "rest of the text on the next line");
  }
  // Typing line shows what you are typing, with special symbols as ASCII
  {
    App a;
    a.handle(Key::COMMA, 0); a.sel = 0; a.handle(Key::EQ, 0);
    a.handle(Key::K4, 0); a.handle(Key::MUL, 0); a.handle(Key::K2, 0);
    lines.clear();
    screen::forEachChatLine(a, grab);
    CHECK(lines.size() == 1 && lines[0] == pad("4x2"), "typing line shows 4x2");
  }
  // Empty chat shows "No messages"
  {
    App a;
    a.handle(Key::COMMA, 0); a.sel = 0; a.handle(Key::EQ, 0);
    lines.clear();
    screen::forEachChatLine(a, grab);
    CHECK(lines.size() == 1 && lines[0].find("No messages") != std::string::npos, "empty chat says No messages");
  }
  // Header: chat name, and the unread square when something is unread
  {
    App a;
    a.handle(Key::COMMA, 0); a.sel = 0; a.handle(Key::EQ, 0);
    lines.clear();
    screen::forEachChatHeader(a, grab);
    CHECK(lines.size() == 1 && lines[0].substr(0, 3) == "ALL" && lines[0][20] != '#', "header shows ALL, no square");
    a.onReceive(pkt(peer(0), "x"), 50);     // another board writes to me, but I'm in ALL
    lines.clear();
    screen::forEachChatHeader(a, grab);
    CHECK(lines[0][20] == '#', "header square when something is unread");
  }
  // Scrolled view shows the right lines (each incoming bubble is 6 lines)
  {
    App a;
    a.handle(Key::COMMA, 0); a.sel = 0; a.handle(Key::EQ, 0);
    for (int i = 0; i < 10; i++) {
      comm::Packet p{}; p.from = peer(0); p.to = comm::ALL_ID; p.len = 40; memset(p.text, 'a' + i, 40); p.text[40] = 0;
      a.onReceive(p, 1000 + i);
    }
    lines.clear();
    screen::forEachChatLine(a, grab);
    CHECK(lines.size() == 6, "bottom view shows 6 lines");
    CHECK(lines[5] == pad(""), "last line is the typing line");
    // Scroll up 2: last shown line is the third text line of the last bubble
    a.handle(Key::LP, 0); a.handle(Key::LP, 0);
    lines.clear();
    screen::forEachChatLine(a, grab);
    CHECK(lines.size() == 6, "scrolled view has 6 lines");
    CHECK(lines[5] == pad(std::string(5, ' ') + "|" + std::string(12, 'j') + "  |"),
          "scrolled view shows the right bubble line");
  }
  // Calculator: short text is one row at the largest size, right-aligned
  {
    std::vector<std::string> rows;
    auto grabRow = [&](const char* s) { rows.push_back(s); };
    screen::forEachCalcLine("2+3", false, grabRow);
    CHECK(rows.size() == 1 && rows[0] == std::string(screen::CALC_LEVELS[0].colsPerRow - 3, ' ') + "2+3", "short text on one row, right-aligned");
    CHECK(rows.size() == 1 && rows[0].size() == screen::CALC_LEVELS[0].colsPerRow, "one row uses the largest size");
  }
  // Calculator: longer text moves to two rows at the middle size
  {
    std::vector<std::string> rows;
    auto grabRow = [&](const char* s) { rows.push_back(s); };
    std::string text(10, 'a');
    screen::forEachCalcLine(text.c_str(), false, grabRow);
    CHECK(rows.size() == 2, "longer text uses two rows");
    CHECK(rows[0] == std::string(8, 'a'), "first row is full at the middle size");
    CHECK(rows[1] == std::string(6, ' ') + "aa", "second row right-aligned");
  }
  // Calculator: a full screen is three rows at the smallest size
  {
    std::vector<std::string> rows;
    auto grabRow = [&](const char* s) { rows.push_back(s); };
    std::string text(screen::CALC_MAX, 'b');
    screen::forEachCalcLine(text.c_str(), false, grabRow);
    CHECK(rows.size() == 3, "full calculator is three rows");
    CHECK(screen::calcLevelFor(text.size(), false).fontHeight == 20, "three rows use the smallest font");
  }
  // Calculator: error is one line
  {
    std::vector<std::string> rows;
    auto grabRow = [&](const char* s) { rows.push_back(s); };
    screen::forEachCalcLine("Syntax error", true, grabRow);
    CHECK(rows.size() == 1 && rows[0] == "Syntax error", "error is one line");
  }
  printf("%d passed, %d failed\n", passes, fails);
  return fails ? 1 : 0;
}
