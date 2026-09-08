#include "prelude/sensors_tlv.h"
#include "prelude/opcodes.h"

namespace prelude {

size_t encodeSensorTlv(const SensorSnapshot& s, uint8_t* out, size_t cap) {
  const size_t need = (s.hasSht4x ? 8 : 0) + (s.hasBattery ? 5 : 0);
  if (cap < need) return 0;
  size_t n = 0;
  if (s.hasSht4x) {
    out[n++] = static_cast<uint8_t>(SensorType::Temperature);
    out[n++] = 2;
    out[n++] = (uint8_t)(s.tempCenti & 0xFF);
    out[n++] = (uint8_t)((uint16_t)s.tempCenti >> 8);
    out[n++] = static_cast<uint8_t>(SensorType::Humidity);
    out[n++] = 2;
    out[n++] = (uint8_t)(s.humCenti & 0xFF);
    out[n++] = (uint8_t)(s.humCenti >> 8);
  }
  if (s.hasBattery) {
    out[n++] = static_cast<uint8_t>(SensorType::Battery);
    out[n++] = 3;
    out[n++] = s.batteryPercent;
    out[n++] = (uint8_t)(s.batteryMv & 0xFF);
    out[n++] = (uint8_t)(s.batteryMv >> 8);
  }
  return n;
}

}  // namespace prelude
