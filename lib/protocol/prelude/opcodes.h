#pragma once
#include <cstddef>
#include <cstdint>

namespace prelude {

constexpr uint16_t kScreenWidth  = 800;
constexpr uint16_t kScreenHeight = 480;
constexpr uint32_t kFrameBytes   = 48000;   // 800 * 480 / 8
constexpr size_t   kMaxStatusText = 500;

enum class Opcode : uint8_t {
  DisplayStatus       = 0x01,
  FrameBegin          = 0x02,
  FrameEnd            = 0x03,
  BuzzerOff           = 0x10,
  BuzzerOn            = 0x11,
  BuzzerOnDismissable = 0x12,
};

enum class EventType : uint8_t {
  Button          = 0x01,
  BuzzerDismissed = 0x02,
  Ack             = 0x03,
};

enum class AckStatus : uint8_t {
  Ok          = 0,
  BadArg      = 1,
  Busy        = 2,
  CrcMismatch = 3,
  Incomplete  = 4,
};

enum class ButtonId : uint8_t { Left = 0, Right = 1, Green = 2 };

enum class SensorType : uint8_t { Temperature = 0x01, Humidity = 0x02, Battery = 0x03 };

}  // namespace prelude
