// Shared test helper. Tests pick other boards with peer(k) instead of fixed numbers,
// so they pass whichever DEVICE_ID the test is built with.
#pragma once
#include "comm.h"

// The k-th other calculator's id (k = 0, 1, 2), skipping this board.
constexpr uint8_t peer(int k) {
  int n = 0;
  for (uint8_t id = 1; id <= 4; id++) {
    if (id == DEVICE_ID) continue;
    if (n++ == k) return id;
  }
  return 0;
}
