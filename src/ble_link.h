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
const uint8_t* frameData();

}  // namespace ble_link
