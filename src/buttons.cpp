#include "buttons.h"
#include <Arduino.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "board_pins.h"
#include "log.h"

namespace buttons {
namespace {

struct RawEdge { uint8_t pin; uint32_t us; };
struct Btn { uint8_t pin; prelude::ButtonId id; const char* name; uint32_t lastUs; };

constexpr uint32_t kDebounceUs = 40000;

QueueHandle_t g_edges = nullptr;
PressHandler g_handler = nullptr;
Btn g_btns[] = {
    {PIN_BTN_LEFT, prelude::ButtonId::Left, "LEFT", 0},
    {PIN_BTN_RIGHT, prelude::ButtonId::Right, "RIGHT", 0},
    {PIN_BTN_GREEN, prelude::ButtonId::Green, "GREEN", 0},
};

void IRAM_ATTR isrButton(void* arg) {
  RawEdge e{(uint8_t)(uintptr_t)arg, (uint32_t)esp_timer_get_time()};
  BaseType_t woken = pdFALSE;
  xQueueSendFromISR(g_edges, &e, &woken);
  portYIELD_FROM_ISR(woken);
}

void buttonTask(void*) {
  RawEdge e;
  for (;;) {
    if (xQueueReceive(g_edges, &e, portMAX_DELAY) != pdTRUE) continue;
    for (Btn& b : g_btns) {
      if (b.pin != e.pin) continue;
      if (e.us - b.lastUs < kDebounceUs) break;
      b.lastUs = e.us;
      LOG("button: %s", b.name);
      if (g_handler) g_handler(b.id);
      break;
    }
  }
}

}  // namespace

void begin(PressHandler onPress) {
  g_handler = onPress;
  g_edges = xQueueCreate(16, sizeof(RawEdge));
  for (Btn& b : g_btns) {
    pinMode(b.pin, INPUT_PULLUP);
    attachInterruptArg(digitalPinToInterrupt(b.pin), isrButton, (void*)(uintptr_t)b.pin, FALLING);
  }
  xTaskCreatePinnedToCore(buttonTask, "buttons", 4096, nullptr, 5, nullptr, 1);
}

bool isGreenHeld() {
  pinMode(PIN_BTN_GREEN, INPUT_PULLUP);
  return digitalRead(PIN_BTN_GREEN) == LOW;
}

}  // namespace buttons
