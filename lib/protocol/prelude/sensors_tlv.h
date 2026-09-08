#pragma once
#include <cstddef>
#include <cstdint>

namespace prelude {
struct SensorSnapshot {
  bool hasSht4x;
  int16_t tempCenti;
  uint16_t humCenti;
  bool hasBattery;
  uint8_t batteryPercent;
  uint16_t batteryMv;
};
constexpr size_t kMaxSensorTlv = 4 + 4 + 5;
// Returns bytes written, or 0 if cap is too small for the present sensors.
size_t encodeSensorTlv(const SensorSnapshot& s, uint8_t* out, size_t cap);
}  // namespace prelude
