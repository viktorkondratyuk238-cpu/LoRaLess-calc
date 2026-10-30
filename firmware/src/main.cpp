// Calculator firmware: keypad (MCP23017) + calculator logic.
// Calculator firmware. The keypad drives the screen logic (ui.h); the OLED and ESP-NOW code is
// compiled only on the chip. On the computer, output goes to the USB serial port.

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui.h"
#include "screen_text.h"
#include "display_hw.h"
#include "radio_hw.h"

static ui::App app;    // the screen logic (moved up so showState can use it)

// ---------- hardware ----------
static const uint8_t MCP_ADDR = 0x20;     // A0, A1, A2 tied to GND
static const int I2C_SDA = 4;             // ESP32 IO4
static const int I2C_SCL = 5;             // ESP32 IO5
static const uint8_t ROW_PIN[6] = {0, 1, 2, 3, 4, 5};     // GPA0..GPA5, driven low one at a time
static const uint8_t COL_PIN[5] = {8, 9, 10, 11, 12};     // GPB0..GPB4, inputs with pull-ups
static const uint8_t UNUSED_PIN[5] = {6, 7, 13, 14, 15}; // GPA6, GPA7, GPB5..GPB7: driven low so they never float
static const uint8_t DEBOUNCE = 3;        // scans a key must stay in the same state

Adafruit_MCP23X17 mcp;

enum Key : uint8_t {
  K_NONE,
  K_0, K_1, K_2, K_3, K_4, K_5, K_6, K_7, K_8, K_9, K_DOT,
  K_ADD, K_SUB, K_MUL, K_DIV, K_POW, K_SQ, K_SQRT, K_EQ, K_LP, K_RP,
  K_C, K_DEL, K_MEMW, K_MR, K_MC, K_COMP, K_FD, K_COMMA, K_SIGN
};

// Layout (row 1 at top, column 1 at left). The sign key is where COMM B used to be.
static const Key KEYMAP[6][5] = {
  {K_C,   K_DEL,  K_SIGN, K_MEMW, K_COMMA},
  {K_SQ,  K_SQRT, K_POW,  K_MR,   K_MC},
  {K_7,   K_8,    K_9,    K_COMP, K_FD},
  {K_4,   K_5,    K_6,    K_LP,   K_RP},
  {K_1,   K_2,    K_3,    K_MUL,  K_DIV},
  {K_0,   K_DOT,  K_EQ,   K_ADD,  K_SUB},
};

// ---------- numbers: exact fraction when possible, double otherwise ----------
struct Val {
  bool exact;
  long long n, d;   // value = n/d when exact (not reduced, so COMP can scale it)
  double v;         // value when not exact
};

static bool gErr = false;

static Val mkExact(long long n, long long d = 1) { return Val{true, n, d, 0.0}; }
static Val mkDbl(double x) { return Val{false, 0, 1, x}; }
static double toD(const Val& a) { return a.exact ? (double)a.n / (double)a.d : a.v; }

static Val vNeg(Val a) { return a.exact ? mkExact(-a.n, a.d) : mkDbl(-a.v); }

static Val vAdd(Val a, Val b) {
  if (a.exact && b.exact) {
    long long x, y, s, q;
    if (!__builtin_mul_overflow(a.n, b.d, &x) && !__builtin_mul_overflow(b.n, a.d, &y) &&
        !__builtin_add_overflow(x, y, &s) && !__builtin_mul_overflow(a.d, b.d, &q))
      return mkExact(s, q);
  }
  return mkDbl(toD(a) + toD(b));
}

static Val vSub(Val a, Val b) { return vAdd(a, vNeg(b)); }

static Val vMul(Val a, Val b) {
  if (a.exact && b.exact) {
    long long p, q;
    if (!__builtin_mul_overflow(a.n, b.n, &p) && !__builtin_mul_overflow(a.d, b.d, &q))
      return mkExact(p, q);
  }
  return mkDbl(toD(a) * toD(b));
}

static Val vDiv(Val a, Val b) {
  if (toD(b) == 0.0) { gErr = true; return mkDbl(0); }
  if (a.exact && b.exact) {
    long long p, q;
    if (!__builtin_mul_overflow(a.n, b.d, &p) && !__builtin_mul_overflow(a.d, b.n, &q)) {
      if (q < 0) { p = -p; q = -q; }
      return mkExact(p, q);
    }
  }
  return mkDbl(toD(a) / toD(b));
}

static Val vPow(Val a, Val b) {
  if (b.exact && b.d == 1 && llabs(b.n) <= 64) {   // whole exponent: keep exact
    Val r = mkExact(1);
    for (long long i = 0; i < llabs(b.n); i++) r = vMul(r, a);
    if (b.n < 0) r = vDiv(mkExact(1), r);
    return r;
  }
  double x = pow(toD(a), toD(b));
  if (isnan(x) || isinf(x)) { gErr = true; return mkDbl(0); }
  return mkDbl(x);
}

static long long isqrtLL(long long x) {
  long long r = (long long)sqrtl((long double)x);
  while (r * r > x) r--;
  while ((r + 1) * (r + 1) <= x) r++;
  return r;
}

static Val vSqrt(Val a) {
  double dv = toD(a);
  if (dv < 0) { gErr = true; return mkDbl(0); }
  if (a.exact && a.n >= 0) {
    long long sn = isqrtLL(a.n), sd = isqrtLL(a.d);
    if (sn * sn == a.n && sd * sd == a.d) return mkExact(sn, sd);
  }
  return mkDbl(sqrt(dv));
}

static long long gcdLL(long long a, long long b) {
  while (b) { long long t = a % b; a = b; b = t; }
  return a;
}

static Val parseEntry(const char* s) {
  long long n = 0, d = 1;
  bool dot = false, neg = false;
  if (*s == '-') { neg = true; s++; }
  for (const char* p = s; *p; p++) {
    if (*p == '.') { dot = true; continue; }
    n = n * 10 + (*p - '0');
    if (dot) d *= 10;
  }
  return mkExact(neg ? -n : n, d);
}

// Display: decimal by default, or n/d (reduced with F<->D, raw with COMP).
enum FMode : uint8_t { M_DEC, M_RED, M_RAW };
static FMode fmode = M_DEC;

static void fmtVal(const Val& a, char* out, size_t sz) {
  if (!a.exact || fmode == M_DEC) { snprintf(out, sz, "%.10g", toD(a)); return; }
  long long n = a.n, d = a.d;
  if (fmode == M_RED) {
    long long g = gcdLL(llabs(n), llabs(d));
    if (g > 0) { n /= g; d /= g; }
  }
  snprintf(out, sz, "%lld/%lld", n, d);
}

// ---------- expression tokens ----------
enum TokT : uint8_t { T_NUM, T_OP, T_LP, T_RP };
struct Tok { TokT t; char op; Val v; };

static const int MAXT = 48;
static Tok toks[MAXT];
static int ntok = 0;

static char entry[17];
static int nent = 0;

static bool pendingSqrt = false;   // √ pressed before a number
static bool resultShown = false;
static bool compPending = false;
static Val lastRes = mkExact(0);

static Val mem = mkExact(0);
static bool memSet = false;

static bool pushTok(TokT t, char op = 0, Val v = mkExact(0)) {
  if (ntok >= MAXT) { gErr = true; return false; }
  toks[ntok++] = Tok{t, op, v};
  return true;
}

static void flushEntry() {
  if (nent == 0) return;
  entry[nent] = 0;
  Val v = parseEntry(entry);
  if (pendingSqrt) { v = vSqrt(v); pendingSqrt = false; }
  pushTok(T_NUM, 0, v);
  nent = 0;
  entry[0] = 0;
}

// Inserts an implicit × before an operand that follows ")" or a number.
static void implicitMulIfNeeded() {
  if (ntok > 0 && (toks[ntok - 1].t == T_RP || toks[ntok - 1].t == T_NUM))
    pushTok(T_OP, '*');
}

static void clearCalc() {
  ntok = 0;
  nent = 0;
  entry[0] = 0;
  pendingSqrt = false;
  resultShown = false;
  compPending = false;
  fmode = M_DEC;
  gErr = false;
}

// ---------- evaluation (shunting-yard) ----------
static int prec(char op) {
  if (op == '+' || op == '-') return 1;
  if (op == '*' || op == '/') return 2;
  return 3;   // '^'
}

static Val applyOp(char op, Val a, Val b) {
  switch (op) {
    case '+': return vAdd(a, b);
    case '-': return vSub(a, b);
    case '*': return vMul(a, b);
    case '/': return vDiv(a, b);
    default:  return vPow(a, b);
  }
}

static bool reduceTop(Val* vs, int& vn, char op) {
  if (vn < 2) return false;
  Val b = vs[--vn];
  Val a = vs[--vn];
  vs[vn++] = applyOp(op, a, b);
  return true;
}

static bool evaluate(Val& out) {
  Val vs[MAXT];
  int vn = 0;
  char os[MAXT];
  int on = 0;

  for (int i = 0; i < ntok; i++) {
    Tok& k = toks[i];
    if (k.t == T_NUM) {
      vs[vn++] = k.v;
    } else if (k.t == T_LP) {
      os[on++] = '(';
    } else if (k.t == T_RP) {
      while (on > 0 && os[on - 1] != '(') {
        if (!reduceTop(vs, vn, os[--on])) return false;
      }
      if (on == 0) return false;   // unbalanced ")"
      on--;                        // drop "("
    } else {                       // operator, '^' is right-associative
      while (on > 0 && os[on - 1] != '(' &&
             (prec(os[on - 1]) > prec(k.op) ||
              (prec(os[on - 1]) == prec(k.op) && k.op != '^'))) {
        if (!reduceTop(vs, vn, os[--on])) return false;
      }
      os[on++] = k.op;
    }
  }
  while (on > 0) {
    if (os[on - 1] == '(') return false;   // unbalanced "("
    if (!reduceTop(vs, vn, os[--on])) return false;
  }
  if (vn != 1) return false;
  out = vs[0];
  return !gErr;
}

// ---------- COMP: scale the last fraction by a whole number ----------
static void applyComp() {
  compPending = false;
  if (nent == 0) return;
  entry[nent] = 0;
  Val k = parseEntry(entry);
  nent = 0;
  entry[0] = 0;
  if (!lastRes.exact || !k.exact || k.d != 1 || k.n <= 0) return;   // non-whole: unchanged
  // Scale the base (reduced) fraction, so 10/5 -> 2/1 -> COMP 5 -> 10/5.
  long long g = gcdLL(llabs(lastRes.n), llabs(lastRes.d));
  if (g > 0) { lastRes.n /= g; lastRes.d /= g; }
  long long n, d;
  if (__builtin_mul_overflow(lastRes.n, k.n, &n) || __builtin_mul_overflow(lastRes.d, k.n, &d)) {
    gErr = true;
    return;
  }
  lastRes.n = n;
  lastRes.d = d;
  fmode = M_RAW;
}

// ---------- key handling ----------
static void showState();
static void calcKey(Key k);

static void calcKey(Key k) {
  if (gErr) clearCalc();

  bool isDigit = (k >= K_0 && k <= K_9);
  if (isDigit || k == K_DOT) {
    if (resultShown && !compPending) clearCalc();   // typing after = starts a new calculation
    if (nent == 0 && !compPending) implicitMulIfNeeded();
    if (nent < 15) {
      if (k == K_DOT) {
        if (strchr(entry, '.') == nullptr) {
          if (nent == 0) entry[nent++] = '0';
          entry[nent++] = '.';
        }
      } else {
        entry[nent++] = (char)('0' + (k - K_0));
      }
      entry[nent] = 0;
    }
    showState();
    return;
  }

  if (k != K_EQ && k != K_COMP) compPending = false;

  switch (k) {
    case K_ADD: case K_SUB: case K_MUL: case K_DIV: case K_POW: {
      char op = (k == K_ADD) ? '+' : (k == K_SUB) ? '-' : (k == K_MUL) ? '*' : (k == K_DIV) ? '/' : '^';
      if (resultShown) {                       // chain from the last result
        ntok = 0;
        pushTok(T_NUM, 0, lastRes);
        resultShown = false;
      }
      flushEntry();
      if (ntok == 0 || toks[ntok - 1].t == T_LP) pushTok(T_NUM, 0, mkExact(0));  // leading operator starts from 0
      if (toks[ntok - 1].t == T_OP) toks[ntok - 1].op = op;                      // replace operator
      else pushTok(T_OP, op);
      break;
    }

    case K_LP:
      if (resultShown) clearCalc();
      flushEntry();
      if (pendingSqrt) { gErr = true; break; }
      implicitMulIfNeeded();
      pushTok(T_LP);
      break;

    case K_RP:
      if (resultShown) break;
      flushEntry();
      pushTok(T_RP);
      break;

    case K_EQ:
      if (compPending) { applyComp(); break; }
      flushEntry();
      if (ntok == 0) break;
      {
        Val r;
        bool ok = evaluate(r);
        ntok = 0;
        if (!ok) { gErr = true; break; }
        lastRes = r;
        resultShown = true;
        fmode = M_DEC;
      }
      break;

    case K_C:
      clearCalc();
      break;

    case K_DEL:
      if (nent > 0) { entry[--nent] = 0; }
      else if (ntok > 0 && !resultShown) { ntok--; }
      break;

    case K_SQ:
      flushEntry();
      if (resultShown) { ntok = 0; pushTok(T_NUM, 0, lastRes); resultShown = false; }
      if (ntok == 0 || toks[ntok - 1].t != T_NUM) { gErr = true; break; }
      toks[ntok - 1].v = vMul(toks[ntok - 1].v, toks[ntok - 1].v);
      break;

    case K_SQRT:
      if (resultShown) {
        ntok = 0;
        pushTok(T_NUM, 0, vSqrt(lastRes));
        resultShown = false;
      } else if (nent > 0) {                   // "16 √" form
        flushEntry();
        if (ntok == 0 || toks[ntok - 1].t != T_NUM) { gErr = true; break; }
        toks[ntok - 1].v = vSqrt(toks[ntok - 1].v);
      } else {                                 // "√ 16" form
        pendingSqrt = true;
      }
      break;

    case K_FD:
      if (resultShown && lastRes.exact) fmode = (fmode == M_DEC) ? M_RED : M_DEC;
      break;

    case K_COMP:
      if (resultShown && lastRes.exact) { compPending = true; nent = 0; entry[0] = 0; }
      break;

    case K_MEMW:
      if (nent > 0) { entry[nent] = 0; mem = parseEntry(entry); memSet = true; }
      else if (resultShown) { mem = lastRes; memSet = true; }
      break;

    case K_MR:
      if (!memSet) break;
      if (resultShown) clearCalc();
      flushEntry();
      implicitMulIfNeeded();
      pushTok(T_NUM, 0, mem);
      break;

    case K_MC:
      memSet = false;
      break;

    case K_COMMA:                                 // COMM A opens contacts; handled by the screen logic
      break;

    case K_SIGN:                                 // +/-: flip the number being typed, or the last result
      if (nent > 0) {
        if (entry[0] == '-') { memmove(entry, entry + 1, nent); nent--; }
        else if (nent < 16) { memmove(entry + 1, entry, nent + 1); entry[0] = '-'; nent++; }
      } else if (resultShown) {
        lastRes = vNeg(lastRes);
      } else if (ntok > 0 && toks[ntok - 1].t == T_NUM) {
        toks[ntok - 1].v = vNeg(toks[ntok - 1].v);
      } else {                                    // nothing yet: start a negative number
        entry[0] = '-'; entry[1] = 0; nent = 1;
      }
      break;

    default:
      break;
  }
  showState();
}

// ---------- output (serial for now; replace with the OLED later) ----------
// The expression as text, e.g. "2 + 3 * 4" or "(2 + 3)".
static void buildExpr(char* line, size_t cap) {
  line[0] = 0;
  char tmp[32];
  for (int i = 0; i < ntok; i++) {
    Tok& k = toks[i];
    if (k.t == T_NUM) snprintf(tmp, sizeof tmp, "%.10g", toD(k.v));
    else if (k.t == T_LP) strcpy(tmp, "(");
    else if (k.t == T_RP) strcpy(tmp, ")");
    else snprintf(tmp, sizeof tmp, " %c ", k.op);
    strncat(line, tmp, cap - strlen(line) - 1);
  }
  if (nent > 0) strncat(line, entry, cap - strlen(line) - 1);
}

// The text the calculator screen shows: the result, "Syntax error", or the expression being typed.
static void calcText(char* text, size_t cap) {
  text[0] = 0;
  if (gErr) strncpy(text, "Syntax error", cap - 1);
  else if (resultShown) fmtVal(lastRes, text, cap);
  else buildExpr(text, cap);
}

// The calculator screen: the expression in rows, or the result / "Syntax error" in its place.
static void showState() {
  char text[160];
  calcText(text, sizeof text);

#ifdef ARDUINO
  hw::drawCalc(text, gErr, app.msg.hasUnread());
  if (compPending) { /* COMP hint is not drawn yet */ }
#else
  int row = 0;
  bool unread = app.msg.hasUnread();
  screen::forEachCalcLine(text, gErr, [&](const char* line) {
    Serial.printf("[%s]%s\n", line, (row == 0 && unread) ? " *" : "");   // * = unread square
    row++;
  });
  if (compPending) Serial.println("COMP: type a whole number, then =");
#endif
}

// Keys that add to the expression. Once the calculator is full, these are ignored.
static bool addsToExpression(Key k) {
  switch (k) {
    case K_0: case K_1: case K_2: case K_3: case K_4:
    case K_5: case K_6: case K_7: case K_8: case K_9:
    case K_DOT: case K_ADD: case K_SUB: case K_MUL: case K_DIV:
    case K_POW: case K_SQ: case K_SQRT: case K_LP: case K_RP: case K_SIGN:
      return true;
    default:
      return false;
  }
}

// ---------- screens and routing ----------

static ui::Key toUi(Key k) {
  switch (k) {
    case K_NONE: return ui::Key::NONE;
    case K_0: return ui::Key::K0;
    case K_1: return ui::Key::K1;
    case K_2: return ui::Key::K2;
    case K_3: return ui::Key::K3;
    case K_4: return ui::Key::K4;
    case K_5: return ui::Key::K5;
    case K_6: return ui::Key::K6;
    case K_7: return ui::Key::K7;
    case K_8: return ui::Key::K8;
    case K_9: return ui::Key::K9;
    case K_DOT: return ui::Key::DOT;
    case K_ADD: return ui::Key::ADD;
    case K_SUB: return ui::Key::SUB;
    case K_MUL: return ui::Key::MUL;
    case K_DIV: return ui::Key::DIV;
    case K_POW: return ui::Key::POW;
    case K_SQ: return ui::Key::SQ;
    case K_SQRT: return ui::Key::SQRT;
    case K_EQ: return ui::Key::EQ;
    case K_LP: return ui::Key::LP;
    case K_RP: return ui::Key::RP;
    case K_C: return ui::Key::C;
    case K_DEL: return ui::Key::DEL;
    case K_MEMW: return ui::Key::MEMW;
    case K_MR: return ui::Key::MR;
    case K_MC: return ui::Key::MC;
    case K_COMP: return ui::Key::COMP;
    case K_FD: return ui::Key::FD;
    case K_COMMA: return ui::Key::COMMA;
    case K_SIGN: return ui::Key::SIGN;
    default: return ui::Key::NONE;
  }
}

// Prints the contacts or chat screen (the calculator prints its own state).
static void printScreen() {
#ifdef ARDUINO
  if (app.screen == ui::Screen::CONTACTS) hw::drawContacts(app);
  else if (app.screen == ui::Screen::CHAT) hw::drawChat(app);
#else
  if (app.screen == ui::Screen::CONTACTS) {
    Serial.println("-- CONTACTS --");
    screen::forEachContactLine(app, [](const char* s) { Serial.println(s); });
  } else if (app.screen == ui::Screen::CHAT) {
    screen::forEachChatHeader(app, [](const char* s) { Serial.println(s); });
    screen::forEachChatLine(app, [](const char* s) { Serial.println(s); });
  }
#endif
}

// Radio stand-in for now: prints what would be sent. ESP-NOW replaces this later.
static void sendPacket(const comm::Packet& p) {
#ifdef ARDUINO
  radio::send(p);
#else
  char text[comm::MAX_MSG_LEN + 1];
  screen::toAscii(text, p.text, p.len);
  if (p.to == comm::ALL_ID) Serial.printf("SENT to ALL: %s\n", text);
  else Serial.printf("SENT to CALC %u: %s\n", (unsigned)p.to, text);
#endif
}

// Every key goes through the screen logic first. Calculator keys go on to calcKey.
static void onKey(Key k) {
  if (app.screen == ui::Screen::CALC && !resultShown && addsToExpression(k)) {
    char expr[160];
    buildExpr(expr, sizeof expr);
    if (strlen(expr) >= screen::CALC_MAX) return;     // full: 3 rows used, no more characters
  }
  ui::Result r = app.handle(toUi(k), millis());
  if (r == ui::Result::FORWARD_TO_CALC) { calcKey(k); return; }
  if (r == ui::Result::SEND) sendPacket(app.outbox);
  if (app.screen == ui::Screen::CALC) showState();
  else printScreen();
}

// A packet from the radio (ESP-NOW will call this later).
static void onRadioPacket(const comm::Packet& p) {
  app.onReceive(p, millis());
  if (app.screen != ui::Screen::CALC) printScreen();
}

// ---------- keypad scan ----------
static bool rawLast[6][5];
static bool stable[6][5];
static uint8_t cnt[6][5];

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin(I2C_SDA, I2C_SCL);
#ifdef ARDUINO
  hw::begin();                 // OLED
  radio::begin();              // ESP-NOW
#endif
  if (!mcp.begin_I2C(MCP_ADDR, &Wire)) {
    Serial.println("MCP23017 not found: check I2C wiring and address");
    while (true) delay(1000);
  }
  for (int r = 0; r < 6; r++) {
    mcp.pinMode(ROW_PIN[r], OUTPUT);
    mcp.digitalWrite(ROW_PIN[r], HIGH);
  }
  for (int c = 0; c < 5; c++) mcp.pinMode(COL_PIN[c], INPUT_PULLUP);
  for (uint8_t p : UNUSED_PIN) {
    mcp.pinMode(p, OUTPUT);
    mcp.digitalWrite(p, LOW);
  }
  Serial.println("Calculator ready");
}

void loop() {
#ifdef ARDUINO
  radio::drain(onRadioPacket);   // packets that arrived since the last loop
#endif
  for (int r = 0; r < 6; r++) {
    mcp.digitalWrite(ROW_PIN[r], LOW);
    delayMicroseconds(30);
    uint8_t b = mcp.readGPIOB();
    mcp.digitalWrite(ROW_PIN[r], HIGH);

    for (int c = 0; c < 5; c++) {
      bool pressed = !(b & (1 << c));        // columns read low when a key in this row is down
      if (pressed != rawLast[r][c]) {
        rawLast[r][c] = pressed;
        cnt[r][c] = 0;
      } else if (cnt[r][c] < DEBOUNCE && ++cnt[r][c] == DEBOUNCE) {
        if (pressed != stable[r][c]) {
          stable[r][c] = pressed;
          if (pressed) onKey(KEYMAP[r][c]);   // act on press only
        }
      }
    }
  }
  delay(5);
}
