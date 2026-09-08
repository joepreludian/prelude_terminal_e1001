#pragma once
#include <cstdint>

namespace sensors {
bool begin();  // starts I2C, probes the SHT4x; returns true if present
bool readSht4x(int16_t& tempCenti, uint16_t& humCenti);
}  // namespace sensors
