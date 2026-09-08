#pragma once
#include <cstddef>
#include <cstdint>

namespace prelude {
constexpr uint8_t kSht4xAddress = 0x44;
constexpr uint8_t kSht4xMeasureHighPrecision = 0xFD;  // then wait >= 10 ms, read 6 bytes
uint8_t sht4xCrc8(const uint8_t* data, size_t len);
// raw = T_msb T_lsb T_crc RH_msb RH_lsb RH_crc. False on CRC error.
bool decodeSht4x(const uint8_t raw[6], int16_t& tempCenti, uint16_t& humCenti);
}  // namespace prelude
