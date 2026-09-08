#include <Arduino.h>
#include <cstring>
#include "battery.h"
#include "board_pins.h"
#include "buttons.h"
#include "buzzer.h"
#include "display.h"
#include "log.h"
#include "prelude/battery_curve.h"
#include "prelude/opcodes.h"
#include "sensors.h"
#include "version.h"

static volatile int g_step = 0;
static uint8_t* g_testFrame = nullptr;

static void onPress(prelude::ButtonId id) {
  if (id == prelude::ButtonId::Green) g_step = g_step + 1;
}

static float tempCb() { return 22.0f; }

void setup() {
  Serial.begin(115200);
  delay(300);
  LOG("=== Prelude Terminal fw %s (display smoke) ===", FW_VERSION_STRING);
  battery::begin();
  buzzer::begin();
  sensors::begin();
  display::begin(tempCb);
  buttons::begin(onPress);

  g_testFrame = (uint8_t*)ps_malloc(prelude::kFrameBytes);
  // Checkerboard of 40 px squares, plus a solid black band on the top 20 rows (1 = black).
  for (int y = 0; y < 480; ++y)
    for (int xb = 0; xb < 100; ++xb)
      g_testFrame[y * 100 + xb] = (y < 20) ? 0xFF : ((((xb * 8) / 40) + (y / 40)) & 1 ? 0xFF : 0x00);

  display::PageInfo page{FW_VERSION_STRING, "Prelude-TEST", "Bluetooth Pairing...",
                         prelude::batteryPercentFromMv(battery::readMv())};
  display::drawPairingPage(page);
}

void loop() {
  static int shown = 0;
  if (g_step != shown) {
    shown = g_step;
    if (shown % 3 == 1) {
      const char* msg = "Hello from the overlay box. This message is long enough to wrap onto several lines.";
      display::drawOverlayBox(msg, strlen(msg));
    } else if (shown % 3 == 2) {
      display::blitFrame(g_testFrame);
    } else {
      display::PageInfo page{FW_VERSION_STRING, "Prelude-TEST", "Connected! Waiting for data...",
                             prelude::batteryPercentFromMv(battery::readMv())};
      display::drawPairingPage(page);
    }
  }
  vTaskDelay(pdMS_TO_TICKS(100));
}
