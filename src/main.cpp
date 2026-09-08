#include <Arduino.h>
#include "board_pins.h"
#include "log.h"
#include "version.h"

void setup() {
  Serial.begin(115200);
  delay(300);
  LOG("=== Prelude Terminal fw %s ===", FW_VERSION_STRING);
  LOG("psram free: %u  heap free: %u",
      (unsigned)ESP.getFreePsram(), (unsigned)ESP.getFreeHeap());
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
