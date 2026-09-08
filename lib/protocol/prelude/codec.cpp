#include "prelude/codec.h"

namespace prelude {
namespace {

uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t le32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
void put16(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)(v >> 8);
}

}  // namespace

ParsedCommand parseCommand(const uint8_t* data, size_t len) {
  ParsedCommand c;
  if (data == nullptr || len == 0) return c;
  c.opcode = data[0];
  const uint8_t* payload = data + 1;
  const size_t plen = len - 1;

  switch (static_cast<Opcode>(c.opcode)) {
    case Opcode::DisplayStatus:
      if (plen < 1 || plen > kMaxStatusText) return c;
      c.text = payload;
      c.textLen = (uint16_t)plen;
      c.valid = true;
      return c;
    case Opcode::FrameBegin:
      if (plen != 8) return c;
      c.frameLength = le32(payload);
      c.frameCrc = le32(payload + 4);
      c.valid = true;
      return c;
    case Opcode::FrameEnd:
    case Opcode::BuzzerOff:
    case Opcode::BuzzerOn:
    case Opcode::BuzzerOnDismissable:
      c.valid = (plen == 0);
      return c;
  }
  return c;  // unknown opcode
}

FrameChunk parseFrameChunk(const uint8_t* data, size_t len) {
  FrameChunk ch;
  if (data == nullptr || len < 3) return ch;
  ch.offset = le16(data);
  ch.data = data + 2;
  ch.len = len - 2;
  ch.valid = true;
  return ch;
}

size_t encodeAck(uint8_t opcode, AckStatus status, uint8_t out[3]) {
  out[0] = static_cast<uint8_t>(EventType::Ack);
  out[1] = opcode;
  out[2] = static_cast<uint8_t>(status);
  return 3;
}

size_t encodeButtonEvent(ButtonId id, uint8_t out[2]) {
  out[0] = static_cast<uint8_t>(EventType::Button);
  out[1] = static_cast<uint8_t>(id);
  return 2;
}

size_t encodeBuzzerDismissed(uint8_t out[1]) {
  out[0] = static_cast<uint8_t>(EventType::BuzzerDismissed);
  return 1;
}

size_t encodeInfo(const DeviceInfo& info, uint8_t out[kInfoSize]) {
  out[0] = info.protocolVersion;
  out[1] = info.fwMajor;
  out[2] = info.fwMinor;
  out[3] = info.fwPatch;
  put16(out + 4, info.width);
  put16(out + 6, info.height);
  out[8] = info.batteryPercent;
  put16(out + 9, info.batteryMv);
  return kInfoSize;
}

}  // namespace prelude
