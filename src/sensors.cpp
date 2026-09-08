#include "sensors.h"
#include <Arduino.h>
#include <Wire.h>
#include "board_pins.h"
#include "log.h"
#include "prelude/sht4x_codec.h"

namespace sensors {
namespace {
bool g_present = false;
}

bool readSht4x(int16_t& tempCenti, uint16_t& humCenti) {
  Wire.beginTransmission(prelude::kSht4xAddress);
  Wire.write(prelude::kSht4xMeasureHighPrecision);
  if (Wire.endTransmission() != 0) return false;
  delay(12);
  if (Wire.requestFrom((int)prelude::kSht4xAddress, 6) != 6) return false;
  uint8_t raw[6];
  for (uint8_t& b : raw) b = (uint8_t)Wire.read();
  return prelude::decodeSht4x(raw, tempCenti, humCenti);
}

bool begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  int16_t t;
  uint16_t h;
  g_present = readSht4x(t, h);
  if (g_present) {
    LOG("sensors: SHT4x ok, %d.%02d C %u.%02u %%RH", t / 100, abs(t % 100), h / 100, h % 100);
  } else {
    LOG("sensors: SHT4x not found");
  }
  return g_present;
}

}  // namespace sensors
