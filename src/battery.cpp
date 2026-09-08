#include "battery.h"
#include <Arduino.h>
#include "board_pins.h"

namespace battery {

void begin() {
  pinMode(PIN_BATT_EN, OUTPUT);
  digitalWrite(PIN_BATT_EN, LOW);
  analogReadResolution(12);
}

uint16_t readMv() {
  digitalWrite(PIN_BATT_EN, HIGH);
  delay(10);
  uint32_t sum = 0;
  for (int i = 0; i < 8; ++i) sum += analogReadMilliVolts(PIN_BATT_ADC);
  digitalWrite(PIN_BATT_EN, LOW);
  return (uint16_t)((sum / 8) * 2);
}

}  // namespace battery
