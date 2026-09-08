#include <Arduino.h>
#include "app.h"
#include "buttons.h"
#include "log.h"
#include "version.h"

static bool greenHeldAtBoot() {
  // Sample for 100 ms; require the button to be held the whole time.
  for (int i = 0; i < 10; ++i) {
    if (!buttons::isGreenHeld()) return false;
    delay(10);
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  const bool clearBonds = greenHeldAtBoot();
  delay(300);
  LOG("=== Prelude Terminal fw %s ===", FW_VERSION_STRING);
  LOG("psram free: %u  heap free: %u", (unsigned)ESP.getFreePsram(), (unsigned)ESP.getFreeHeap());
  if (clearBonds) LOG("boot: GREEN held, clearing pairing");
  app::begin(clearBonds);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
