#include "prelude/sht4x_codec.h"

namespace prelude {

uint8_t sht4xCrc8(const uint8_t* data, size_t len) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
  }
  return crc;
}

bool decodeSht4x(const uint8_t raw[6], int16_t& tempCenti, uint16_t& humCenti) {
  if (sht4xCrc8(raw, 2) != raw[2] || sht4xCrc8(raw + 3, 2) != raw[5]) return false;
  const uint16_t st = (uint16_t)((raw[0] << 8) | raw[1]);
  const uint16_t sh = (uint16_t)((raw[3] << 8) | raw[4]);
  // T[C] = -45 + 175 * st / 65535 ; RH[%] = -6 + 125 * sh / 65535 (values in centi units)
  int32_t t = -4500 + (int32_t)((17500LL * st + 32767) / 65535);
  int32_t h = -600 + (int32_t)((12500LL * sh + 32767) / 65535);
  if (h < 0) h = 0;
  if (h > 10000) h = 10000;
  tempCenti = (int16_t)t;
  humCenti = (uint16_t)h;
  return true;
}

}  // namespace prelude
