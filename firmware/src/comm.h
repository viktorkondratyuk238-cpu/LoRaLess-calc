// Messaging backend: chats, multi-tap typing, and the message composer.
// No hardware code in here, so it can be tested on a computer.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef DEVICE_ID
#define DEVICE_ID 1            // set to 1..4 per board before flashing
#endif

namespace comm {

constexpr uint8_t ALL_ID = 0;          // recipient "everyone"; CALC 1..4 are ids 1..4
constexpr uint8_t NUM_SLOTS = 5;       // chat slots: 0 = ALL, 1..4 = CALC n
constexpr uint8_t MAX_CHAT_MSGS = 30;
constexpr uint8_t MAX_MSG_LEN = 120;
constexpr uint32_t MSG_TTL_MS = 60UL * 60UL * 1000UL;   // drop messages older than 60 min
constexpr uint32_t MULTITAP_MS = 1000UL;                // same key within this time cycles letters
constexpr uint32_t DOUBLE_EQ_MS = 500UL;                // two "=" this fast sends

// Symbols that are not plain ASCII. The display draws these as glyphs.
constexpr char SYM_MUL = (char)0x80;   // ×
constexpr char SYM_DIV = (char)0x81;   // ÷
constexpr char SYM_SUB = (char)0x82;   // −
constexpr char SYM_POW = (char)0x83;   // x with a small raised y (power)

// What goes over the radio.
constexpr uint8_t KIND_MSG = 0;        // a chat message
constexpr uint8_t KIND_ACK_OK = 1;     // the receiver stored message `id`
constexpr uint8_t KIND_ACK_FAIL = 2;   // the receiver could not store message `id`

struct Packet {
  uint8_t from;
  uint8_t to;
  uint8_t len;
  uint8_t id;      // message number, 1..255 (0 for ALL, which is never acked)
  uint8_t kind;    // KIND_MSG, KIND_ACK_OK or KIND_ACK_FAIL
  char text[MAX_MSG_LEN + 1];
};

// What gets stored in RAM.
struct Msg {
  uint8_t from;
  uint8_t to;
  uint32_t t;
  uint8_t len;
  char text[MAX_MSG_LEN + 1];
};

// One conversation. Messages are kept in arrival order.
struct Chat {
  Msg msgs[MAX_CHAT_MSGS];
  uint8_t n = 0;
  bool unread = false;

  void prune(uint32_t now) {
    uint8_t start = 0;
    while (start < n && (uint32_t)(now - msgs[start].t) > MSG_TTL_MS) start++;
    if (start) {
      memmove(msgs, msgs + start, (n - start) * sizeof(Msg));
      n -= start;
    }
  }

  void add(const Msg& m, uint32_t now) {
    prune(now);
    if (n == MAX_CHAT_MSGS) {                 // full: drop the oldest
      memmove(msgs, msgs + 1, (n - 1) * sizeof(Msg));
      n--;
    }
    msgs[n++] = m;
  }
};

// Handles incoming and outgoing messages for all five chats.
class Messenger {
 public:
  static constexpr uint8_t NONE = 255;

  Chat chats[NUM_SLOTS];
  uint8_t openSlot = NONE;      // which chat is on screen, if any

  // Returns true if the packet was for this board and was stored.
  bool receive(const Packet& p, uint32_t now) {
    if (p.from < 1 || p.from > 4 || p.from == DEVICE_ID) return false;
    if (p.to != ALL_ID && p.to != DEVICE_ID) return false;
    if (p.len > MAX_MSG_LEN) return false;

    uint8_t slot = (p.to == ALL_ID) ? ALL_ID : p.from;
    chats[slot].add(make(p.from, p.to, p.text, p.len, now), now);
    if (openSlot != slot) chats[slot].unread = true;
    return true;
  }

  // Stores an outgoing message and fills in the packet to send.
  bool send(uint8_t to, const char* text, uint8_t len, uint32_t now, Packet* out) {
    if (to == DEVICE_ID || to > 4 || len == 0 || len > MAX_MSG_LEN) return false;
    chats[to].add(make(DEVICE_ID, to, text, len, now), now);
    out->from = DEVICE_ID;
    out->to = to;
    out->len = len;
    memcpy(out->text, text, len);
    out->text[len] = 0;
    return true;
  }

  void open(uint8_t slot) {
    openSlot = slot;
    chats[slot].unread = false;
  }

  void close() { openSlot = NONE; }

  bool hasUnread() const {
    for (uint8_t i = 0; i < NUM_SLOTS; i++)
      if (chats[i].unread) return true;
    return false;
  }

 private:
  static Msg make(uint8_t from, uint8_t to, const char* text, uint8_t len, uint32_t now) {
    Msg m;
    m.from = from;
    m.to = to;
    m.t = now;
    m.len = len;
    memcpy(m.text, text, len);
    m.text[len] = 0;
    return m;
  }
};

// Sends messages one at a time and waits for an ack from the receiver.
// A message is resent until it is acked or MAX_TRIES is used up.
// Messages to ALL are sent once and are not acked.
class Link {
 public:
  static constexpr uint8_t QUEUE = 4;         // messages waiting to go out
  static constexpr uint8_t MAX_TRIES = 4;     // sends per message before giving up
  static constexpr uint32_t RETRY_MS = 250;   // wait for an ack before resending
  static constexpr uint32_t JITTER_MS = 150;  // random extra wait, so boards don't resend in step
  static constexpr uint32_t DEDUPE_MS = 5000; // a repeat of the last message number within this time is a duplicate

  // Queues a packet and gives it a message number. Returns false if the queue is full.
  bool push(Packet p) {
    if (count == QUEUE) return false;
    if (p.to == ALL_ID) {
      p.id = 0;
    } else {
      p.id = nextId;
      nextId = (nextId == 255) ? 1 : nextId + 1;
    }
    p.kind = KIND_MSG;
    q[count].p = p;
    q[count].tries = 0;
    q[count].nextAt = 0;
    count++;
    return true;
  }

  // Call often. Returns true and fills *out when a packet should be sent now.
  bool poll(uint32_t now, uint32_t rnd, Packet* out) {
    if (!count) return false;
    Item& h = q[0];
    if (h.p.to == ALL_ID) {
      *out = h.p;
      pop();
      return true;
    }
    if (h.tries) {
      if (now < h.nextAt) return false;
      if (h.tries >= MAX_TRIES) {
        pop();
        result = 0;
        return false;
      }
    }
    *out = h.p;
    h.tries++;
    h.nextAt = now + RETRY_MS + (rnd % JITTER_MS);
    return true;
  }

  // Handles an ack from the receiver. A refusal means it will be resent after RETRY_MS.
  void onAck(const Packet& p, uint32_t now) {
    if (!count) return;
    Item& h = q[0];
    if (p.from != h.p.to || p.id != h.p.id) return;
    if (p.kind == KIND_ACK_OK) {
      pop();
      result = 1;
    } else if (p.kind == KIND_ACK_FAIL) {
      h.nextAt = now + RETRY_MS;
    }
  }

  // Handles one packet from the radio. Calls store() for each new message for this board.
  // Returns true if *reply should be sent back (an ack).
  template <typename Store>
  bool incoming(const Packet& p, uint32_t now, Packet* reply, Store store) {
    if (p.kind != KIND_MSG) {
      if (p.to == DEVICE_ID) onAck(p, now);   // acks are broadcast, so ignore other boards' acks
      return false;
    }
    if (p.to == ALL_ID) {
      store(p);
      return false;
    }
    if (p.to != DEVICE_ID || p.from < 1 || p.from > 4) return false;
    // The sender resent it after a lost ack. Only a repeat within DEDUPE_MS counts,
    // so a rebooted sender starting again at message 1 is not mistaken for a repeat.
    bool dup = (p.id == lastRx[p.from]) && (uint32_t)(now - lastRxTime[p.from]) < DEDUPE_MS;
    bool ok = dup || store(p);
    if (ok) {
      lastRx[p.from] = p.id;
      lastRxTime[p.from] = now;
    }
    reply->from = DEVICE_ID;
    reply->to = p.from;
    reply->len = 0;
    reply->id = p.id;
    reply->kind = ok ? KIND_ACK_OK : KIND_ACK_FAIL;
    reply->text[0] = 0;
    return true;
  }

  // Returns true once when a message has finished: true for delivered, false for given up.
  bool takeResult(bool* delivered) {
    if (result < 0) return false;
    *delivered = (result == 1);
    result = -1;
    return true;
  }

 private:
  struct Item {
    Packet p;
    uint8_t tries;
    uint32_t nextAt;
  };
  Item q[QUEUE];
  uint8_t count = 0;
  uint8_t nextId = 1;
  uint8_t lastRx[5] = {0};     // last message number seen from each board (index = board id)
  uint32_t lastRxTime[5] = {0};
  int8_t result = -1;          // -1 none, 0 given up, 1 delivered

  void pop() {
    for (uint8_t i = 1; i < count; i++) q[i - 1] = q[i];
    count--;
  }
};

// The text being typed in a chat. Digit keys use multi-tap. F↔D and COMP work
// on the number being typed, the same way they do on the calculator.
class Composer {
 public:
  char text[MAX_MSG_LEN + 1] = {0};
  uint8_t len = 0;

  // True while COMP is waiting for its whole number (then "=").
  bool compPending() const { return comp.on; }

  void clear() {
    len = 0;
    text[0] = 0;
    resetTap();
  }

  // DEL: remove the last character.
  void backspace() {
    resetTap();
    if (len) text[--len] = 0;
  }

  // Digit keys 0-9. Repeated presses of the same key within a second cycle its letters.
  // While COMP is waiting, digits are the COMP multiplier instead.
  // Returns false if the message is full.
  bool digit(char d, uint32_t now) {
    if (comp.on) {
      if (d >= '0' && d <= '9' && comp.len < 6) comp.buf[comp.len++] = d;
      comp.buf[comp.len] = 0;
      return true;
    }
    const char* set = cycleOf(d);
    if (!set) return false;
    lastEq = false;

    bool cont = tapping && d == lastKey && (uint32_t)(now - lastTime) < MULTITAP_MS;
    if (cont) {
      cyc = (cyc + 1) % strlen(set);
      text[len - 1] = set[cyc];
    } else {
      if (len >= MAX_MSG_LEN) return false;
      cyc = 0;
      text[len++] = set[0];
      text[len] = 0;
      tapping = true;
      lastKey = d;
    }
    lastTime = now;
    return true;
  }

  // Symbol keys (+ − × ÷ ^ ( ) .). Each press types one character.
  bool symbol(char c) {
    resetTap();
    if (len >= MAX_MSG_LEN) return false;
    text[len++] = c;
    text[len] = 0;
    return true;
  }

  // "=" key. While COMP is waiting, "=" applies it and does not send.
  // Otherwise one press types "=", and two presses within 0.5 s send.
  // Returns true when the caller should send.
  bool equals(uint32_t now) {
    if (comp.on) {
      applyComp();
      return false;
    }
    bool prevEq = lastEq;
    tapping = false;
    if (prevEq && (uint32_t)(now - lastEqTime) < DOUBLE_EQ_MS && len > 0 && text[len - 1] == '=') {
      text[--len] = 0;
      lastEq = false;
      return len > 0;      // nothing to send if the message is empty
    }
    if (len >= MAX_MSG_LEN) return false;
    text[len++] = '=';
    text[len] = 0;
    lastEq = true;
    lastEqTime = now;
    return false;
  }

  // x² and √: replace the number being typed with its square or square root.
  // Works on decimals only. Returns false if there is no such number or it does not fit.
  bool replaceLastNumber(bool squareIt) {
    int s = tokenStart();
    if (s == len) return false;
    if (hasSlash(s)) {                              // fraction: square or root top and bottom
      long long a, b;
      if (!readFraction(s, a, b) || b == 0) return false;
      resetTap();
      if (squareIt) {
        char out[32];
        snprintf(out, sizeof out, "%lld/%lld", a * a, b * b);
        return replaceTail(s, out);
      }
      long long ra = isqrtLL(a), rb = isqrtLL(b);
      char out[32];
      if (a >= 0 && ra * ra == a && rb * rb == b && rb > 0)
        snprintf(out, sizeof out, "%lld/%lld", ra, rb);      // exact, like the calculator
      else
        snprintf(out, sizeof out, "%.10g", sqrt((double)a / (double)b));
      return replaceTail(s, out);
    }
    char num[24] = {0};
    uint8_t n = len - s;
    if (n >= sizeof num) return false;
    memcpy(num, text + s, n);
    double x = strtod(num, nullptr);
    double r = squareIt ? x * x : sqrt(x);
    char out[24];
    snprintf(out, sizeof out, "%.10g", r);
    bool ok = replaceTail(s, out);
    resetTap();
    return ok;
  }

  // F↔D on the number being typed: "2.5" becomes "5/2", and "5/2" becomes "2.5".
  bool toggleFraction() {
    int s = tokenStart();
    if (s == len) return false;
    long long a, b;
    if (hasSlash(s)) {
      if (!readFraction(s, a, b) || b == 0) return false;
      long long g = gcd(a, b);
      a /= g; b /= g;
      char out[24];
      snprintf(out, sizeof out, "%.10g", (double)a / (double)b);
      resetTap();
      return replaceTail(s, out);
    }
    if (!readDecimal(s, a, b)) return false;
    long long g = gcd(a, b);
    if (g == 0) return false;
    a /= g; b /= g;
    char out[24];
    snprintf(out, sizeof out, "%lld/%lld", a, b);
    resetTap();
    return replaceTail(s, out);
  }

  // COMP: start scaling the number being typed. Then type a whole number and press "=".
  // The number is first reduced to a fraction, the same as the calculator's base fraction.
  bool startComp() {
    int s = tokenStart();
    if (s == len) return false;
    long long a, b;
    if (hasSlash(s)) {
      if (!readFraction(s, a, b) || b == 0) return false;
    } else {
      if (!readDecimal(s, a, b)) return false;
    }
    long long g = gcd(a, b);
    if (g == 0) return false;
    comp.a = a / g;
    comp.b = b / g;
    comp.start = s;
    comp.len = 0;
    comp.buf[0] = 0;
    comp.on = true;
    tapping = false;
    lastEq = false;
    return true;
  }

  // Adds text such as a calculator result. Returns false if it does not fit.
  bool insert(const char* s) {
    resetTap();
    uint8_t n = (uint8_t)strlen(s);
    if (len + n > MAX_MSG_LEN) return false;
    memcpy(text + len, s, n);
    len += n;
    text[len] = 0;
    return true;
  }

  // The letters a digit key cycles through, starting with the digit itself.
  static const char* cycleOf(char d) {
    switch (d) {
      case '1': return "1.,?!";
      case '2': return "2ABC";
      case '3': return "3DEF";
      case '4': return "4GHI";
      case '5': return "5JKL";
      case '6': return "6MNO";
      case '7': return "7PQRS";
      case '8': return "8TUV";
      case '9': return "9WXYZ";
      case '0': return "0 ";
      default:  return nullptr;
    }
  }

 private:
  struct CompState {
    bool on = false;
    long long a = 0, b = 1;      // base fraction, reduced
    int start = 0;               // where the number starts in text
    char buf[8] = {0};           // the whole number typed after COMP
    uint8_t len = 0;
  } comp;

  // Start of the number being typed at the end: digits, ".", and "/" (for fractions).
  int tokenStart() const {
    int s = len;
    while (s > 0 && isTokenChar(text[s - 1])) s--;
    return s;
  }

  static bool isTokenChar(char c) { return (c >= '0' && c <= '9') || c == '.' || c == '/'; }
  bool hasSlash(int s) const { return memchr(text + s, '/', len - s) != nullptr; }

  static long long isqrtLL(long long x) {
    if (x < 0) return -1;
    long long r = (long long)sqrt((double)x);
    while (r * r > x) r--;
    while ((r + 1) * (r + 1) <= x) r++;
    return r;
  }

  static long long gcd(long long a, long long b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b) { long long t = a % b; a = b; b = t; }
    return a;
  }

  // Reads "a/b" from text[s..].
  bool readFraction(int s, long long& a, long long& b) const {
    char tok[24] = {0};
    uint8_t n = len - s;
    if (n >= sizeof tok) return false;
    memcpy(tok, text + s, n);
    char* slash = strchr(tok, '/');
    if (!slash) return false;
    *slash = 0;
    a = atoll(tok);
    b = atoll(slash + 1);
    return true;
  }

  // Reads a decimal like "2.5" from text[s..] as an exact fraction.
  bool readDecimal(int s, long long& a, long long& b) const {
    a = 0; b = 1;
    bool dot = false;
    for (int i = s; i < len; i++) {
      char c = text[i];
      if (c == '.') { if (dot) return false; dot = true; continue; }
      if (c < '0' || c > '9') return false;
      a = a * 10 + (c - '0');
      if (dot) b *= 10;
      if (b > 1000000000LL || a > 1000000000000LL) return false;
    }
    return true;
  }

  // Replaces text from position s to the end with out.
  bool replaceTail(int s, const char* out) {
    uint8_t m = (uint8_t)strlen(out);
    if (s + m > MAX_MSG_LEN) return false;
    memcpy(text + s, out, m);
    len = s + m;
    text[len] = 0;
    return true;
  }

  // Applies COMP: the base fraction times the typed whole number, e.g. 5/2 x 3 = 15/6.
  void applyComp() {
    long long k = atoll(comp.buf);
    bool ok = comp.len > 0 && k > 0;
    if (ok) {
      char out[32];
      snprintf(out, sizeof out, "%lld/%lld", comp.a * k, comp.b * k);
      ok = replaceTail(comp.start, out);
    }
    comp.on = false;
    comp.len = 0;
    comp.buf[0] = 0;
    lastEq = false;
  }

  void resetTap() {
    tapping = false;
    lastEq = false;
    comp.on = false;                  // any other key cancels a waiting COMP
    comp.len = 0;
    comp.buf[0] = 0;
  }

  bool tapping = false;
  char lastKey = 0;
  uint32_t lastTime = 0;
  uint8_t cyc = 0;
  bool lastEq = false;
  uint32_t lastEqTime = 0;
};

}  // namespace comm
