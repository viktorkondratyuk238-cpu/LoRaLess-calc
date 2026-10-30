#pragma once
#include <stdint.h>
#include "Arduino.h"
struct Adafruit_MCP23X17 {
  bool begin_I2C(uint8_t, void*) { return true; }
  void pinMode(uint8_t, int) {}
  void digitalWrite(uint8_t, int) {}
  uint8_t readGPIOB() { return 0xFF; }
};
