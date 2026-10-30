#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
struct SerialStub {
  void begin(long) {}
  void println(const char* s) { printf("%s\n", s); }
  void printf(const char* f, ...) { va_list a; va_start(a, f); vprintf(f, a); va_end(a); }
};
static SerialStub Serial;
inline void delay(unsigned long) {}
inline void delayMicroseconds(unsigned int) {}
#define OUTPUT 1
#define INPUT_PULLUP 2
#define HIGH 1
#define LOW 0

static unsigned long g_ms = 0;
inline unsigned long millis() { return g_ms; }
