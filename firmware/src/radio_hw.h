// ESP-NOW radio for the chip. UNTESTED: written without the board, so expect small fixes.
// Only compiled on the chip (ARDUINO is defined there). Computer tests skip this file.
#pragma once
#ifdef ARDUINO
#include <WiFi.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "comm.h"

namespace radio {

static QueueHandle_t rxQueue = nullptr;
static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Packets are received on the radio task, so they are queued and handled in loop().
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
static void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  (void)info;
#else
static void onRecv(const uint8_t* mac, const uint8_t* data, int len) {
  (void)mac;
#endif
  if (len != (int)sizeof(comm::Packet)) return;   // not one of ours
  comm::Packet p;
  memcpy(&p, data, sizeof p);
  xQueueSend(rxQueue, &p, 0);
}

inline bool begin() {
  rxQueue = xQueueCreate(8, sizeof(comm::Packet));
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return false;
  }
  esp_now_register_recv_cb(onRecv);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST, 6);
  peer.channel = 0;          // current channel
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("ESP-NOW add peer failed");
    return false;
  }
  return true;
}

// Sends to everyone; each board keeps only what is addressed to it.
inline void send(const comm::Packet& p) {
  esp_now_send(BROADCAST, (const uint8_t*)&p, sizeof p);
}

// Calls handler for each packet that arrived since the last call.
template <typename Handler>
void drain(Handler handler) {
  comm::Packet p;
  while (rxQueue && xQueueReceive(rxQueue, &p, 0) == pdTRUE) handler(p);
}

}  // namespace radio
#endif  // ARDUINO
