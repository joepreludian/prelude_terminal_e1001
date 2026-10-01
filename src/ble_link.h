#pragma once
#include <cstddef>
#include <cstdint>
#include "prelude/codec.h"

namespace ble_link {

struct Callbacks {
  void (*onConnected)(const char* peerAddr);            // after encryption with an accepted peer
  void (*onDisconnected)();
  bool (*onCommand)(const prelude::ParsedCommand& cmd);  // DisplayStatus / Buzzer*; false if queue full
  bool (*onFrameReady)();                                // frame validated in staging; false if queue full
};

struct ConnParams {
  uint16_t minInterval;  // 1.25 ms units
  uint16_t maxInterval;  // 1.25 ms units
  uint16_t latency;      // connection events the peer may skip
  uint16_t timeout;      // 10 ms units
};

void begin(const Callbacks& cb, uint8_t* frameBuffer, bool clearBonds);
const char* deviceName();
bool isBonded();
const char* bondedPeer();  // "AA:BB:CC:DD:EE:FF" or ""
void notifyEvent(const uint8_t* data, size_t len);
void sendAck(uint8_t opcode, prelude::AckStatus status);
void setInfo(const prelude::DeviceInfo& info);
void setSensors(const uint8_t* tlv, size_t len);
void setBatteryLevel(uint8_t percent);
void setRenderBusy(bool busy);
// Preferred connection parameters: applied to current links now and to every
// future link after authentication.
void setConnParams(const ConnParams& p);
const uint8_t* frameData();

}  // namespace ble_link
