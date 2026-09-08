#include "buzzer.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include "board_pins.h"
#include "prelude/buzzer_pattern.h"

namespace buzzer {
namespace {

constexpr uint32_t kToneHz = 2000;
TimerHandle_t g_timer = nullptr;
prelude::BuzzerPattern g_pattern;
prelude::BuzzerMode g_mode = prelude::BuzzerMode::Off;
bool g_toneActive = false;  // noTone() logs an error if no tone is running

void applyTone() {
  if (g_pattern.toneOn()) {
    tone(PIN_BUZZER, kToneHz);
    g_toneActive = true;
  } else if (g_toneActive) {
    noTone(PIN_BUZZER);
    g_toneActive = false;
  }
}

void onTimer(TimerHandle_t) {
  g_pattern.toggle();
  applyTone();
}

}  // namespace

void begin() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  g_timer = xTimerCreate("buzzer", pdMS_TO_TICKS(prelude::BuzzerPattern::kStepMs), pdTRUE, nullptr, onTimer);
}

void setMode(prelude::BuzzerMode mode) {
  g_mode = mode;
  if (mode == prelude::BuzzerMode::Off) {
    xTimerStop(g_timer, 0);
    g_pattern.stop();
    applyTone();
    return;
  }
  g_pattern.start();
  applyTone();
  xTimerReset(g_timer, 0);  // (re)start the 300 ms cadence from now
}

prelude::BuzzerMode mode() { return g_mode; }

}  // namespace buzzer
