#include <Arduino.h>
#include "battery.h"
#include "board_pins.h"
#include "buttons.h"
#include "buzzer.h"
#include "log.h"
#include "prelude/battery_curve.h"
#include "sensors.h"
#include "version.h"

static void onPress(prelude::ButtonId id) {
  if (id == prelude::ButtonId::Green) {
    buzzer::setMode(buzzer::mode() == prelude::BuzzerMode::Off ? prelude::BuzzerMode::On
                                                                 : prelude::BuzzerMode::Off);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  LOG("=== Prelude Terminal fw %s (smoke) ===", FW_VERSION_STRING);
  battery::begin();
  buzzer::begin();
  sensors::begin();
  buttons::begin(onPress);
}

void loop() {
  uint16_t mv = battery::readMv();
  LOG("battery: %u mV (%u%%)", mv, prelude::batteryPercentFromMv(mv));
  int16_t t;
  uint16_t h;
  if (sensors::readSht4x(t, h)) LOG("sht4x: %d.%02d C %u.%02u %%RH", t / 100, abs(t % 100), h / 100, h % 100);
  vTaskDelay(pdMS_TO_TICKS(5000));
}
