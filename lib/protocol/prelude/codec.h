#pragma once
#include <cstddef>
#include <cstdint>
#include "prelude/opcodes.h"

namespace prelude {

struct ParsedCommand {
  bool valid = false;        // false => reply ACK(opcode, BadArg)
  uint8_t opcode = 0;        // raw opcode byte, 0 when input was empty
  uint32_t frameLength = 0;  // FrameBegin
  uint32_t frameCrc = 0;     // FrameBegin
  const uint8_t* text = nullptr;  // DisplayStatus, points into the input buffer
  uint16_t textLen = 0;
  PowerMode powerMode = PowerMode::Performance;  // SetPowerMode
};

struct FrameChunk {
  bool valid = false;
  uint16_t offset = 0;
  const uint8_t* data = nullptr;
  size_t len = 0;
};

struct DeviceInfo {
  uint8_t protocolVersion;
  uint8_t fwMajor, fwMinor, fwPatch;
  uint16_t width, height;
  uint8_t batteryPercent;
  uint16_t batteryMv;
  PowerMode powerMode;
};
constexpr size_t kInfoSize = 12;

ParsedCommand parseCommand(const uint8_t* data, size_t len);
FrameChunk parseFrameChunk(const uint8_t* data, size_t len);

size_t encodeAck(uint8_t opcode, AckStatus status, uint8_t out[3]);
size_t encodeButtonEvent(ButtonId id, uint8_t out[2]);
size_t encodeBuzzerDismissed(uint8_t out[1]);
size_t encodeInfo(const DeviceInfo& info, uint8_t out[kInfoSize]);

}  // namespace prelude
