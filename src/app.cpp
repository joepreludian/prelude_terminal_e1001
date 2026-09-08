#include "app.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <cstdlib>
#include <cstring>
#include "battery.h"
#include "ble_link.h"
#include "buttons.h"
#include "buzzer.h"
#include "display.h"
#include "log.h"
#include "prelude/battery_curve.h"
#include "prelude/button_policy.h"
#include "prelude/codec.h"
#include "prelude/sensors_tlv.h"
#include "sensors.h"
#include "version.h"

namespace app {
namespace {

enum class MsgType : uint8_t { Command, FrameReady, Button, LinkUp, LinkDown, Housekeeping };

struct AppMsg {
  MsgType type;
  uint8_t opcode;
  uint8_t button;
  uint16_t textLen;
  char text[prelude::kMaxStatusText];
  char peer[18];
  prelude::SensorSnapshot sample;
};

enum class LinkState : uint8_t { Pairing, Waiting, Connected };

struct State {
  LinkState link = LinkState::Pairing;
  bool serverOwnsScreen = false;
  prelude::BuzzerMode buzzer = prelude::BuzzerMode::Off;
  prelude::SensorSnapshot sample{false, 0, 0, true, 0, 0};
  uint8_t drawnBatteryPercent = 0;
  uint32_t drawnAtMs = 0;
};

constexpr uint32_t kHousekeepingMs = 30000;

QueueHandle_t g_queue = nullptr;
State g_state;
volatile float g_panelTempC = 16.0f;
bool g_hasSht4x = false;
uint8_t* g_frameBuffer = nullptr;

float panelTemp() { return g_panelTempC; }

bool post(const AppMsg& m, TickType_t wait = 0) {
  return xQueueSend(g_queue, &m, wait) == pdTRUE;
}

// ---- callbacks from other tasks (BLE host task, button task) ----

bool onBleCommand(const prelude::ParsedCommand& cmd) {
  AppMsg m{};
  m.type = MsgType::Command;
  m.opcode = cmd.opcode;
  if (cmd.text && cmd.textLen) {
    m.textLen = cmd.textLen;
    memcpy(m.text, cmd.text, cmd.textLen);
  }
  return post(m);
}

bool onBleFrameReady() {
  AppMsg m{};
  m.type = MsgType::FrameReady;
  return post(m);
}

void onBleConnected(const char* peer) {
  AppMsg m{};
  m.type = MsgType::LinkUp;
  strncpy(m.peer, peer, sizeof m.peer - 1);
  post(m, pdMS_TO_TICKS(100));
}

void onBleDisconnected() {
  AppMsg m{};
  m.type = MsgType::LinkDown;
  post(m, pdMS_TO_TICKS(100));
}

void onButton(prelude::ButtonId id) {
  AppMsg m{};
  m.type = MsgType::Button;
  m.button = static_cast<uint8_t>(id);
  post(m);
}

// ---- helpers (app task only) ----

const char* statusText(char* buf, size_t cap) {
  switch (g_state.link) {
    case LinkState::Pairing:
      return "Bluetooth Pairing...";
    case LinkState::Waiting:
      snprintf(buf, cap, "Paired with %s, waiting for connection...", ble_link::bondedPeer());
      return buf;
    case LinkState::Connected:
      return "Connected! Waiting for data...";
  }
  return "";
}

void drawPage() {
  char buf[80];
  display::PageInfo page{FW_VERSION_STRING, ble_link::deviceName(), statusText(buf, sizeof buf),
                         g_state.sample.batteryPercent};
  display::drawPairingPage(page);
  g_state.drawnBatteryPercent = g_state.sample.batteryPercent;
  g_state.drawnAtMs = millis();
}

void publishSample() {
  prelude::DeviceInfo info{PROTOCOL_VERSION, FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH,
                           prelude::kScreenWidth, prelude::kScreenHeight,
                           g_state.sample.batteryPercent, g_state.sample.batteryMv};
  ble_link::setInfo(info);
  uint8_t tlv[prelude::kMaxSensorTlv];
  size_t n = prelude::encodeSensorTlv(g_state.sample, tlv, sizeof tlv);
  ble_link::setSensors(tlv, n);
  ble_link::setBatteryLevel(g_state.sample.batteryPercent);
}

void setBuzzer(prelude::BuzzerMode mode) {
  g_state.buzzer = mode;
  buzzer::setMode(mode);
}

void handleCommand(const AppMsg& m) {
  switch (static_cast<prelude::Opcode>(m.opcode)) {
    case prelude::Opcode::DisplayStatus:
      LOG("app: display_status (%u bytes)", m.textLen);
      display::drawOverlayBox(m.text, m.textLen);
      ble_link::sendAck(m.opcode, prelude::AckStatus::Ok);
      return;
    case prelude::Opcode::BuzzerOff:
      setBuzzer(prelude::BuzzerMode::Off);
      break;
    case prelude::Opcode::BuzzerOn:
      setBuzzer(prelude::BuzzerMode::On);
      break;
    case prelude::Opcode::BuzzerOnDismissable:
      setBuzzer(prelude::BuzzerMode::Dismissable);
      break;
    default:
      ble_link::sendAck(m.opcode, prelude::AckStatus::BadArg);
      return;
  }
  LOG("app: buzzer mode %u", (unsigned)g_state.buzzer);
  ble_link::sendAck(m.opcode, prelude::AckStatus::Ok);
}

void handleFrameReady() {
  const uint32_t t0 = millis();
  display::blitFrame(ble_link::frameData());
  g_state.serverOwnsScreen = true;
  ble_link::setRenderBusy(false);
  ble_link::sendAck(static_cast<uint8_t>(prelude::Opcode::FrameEnd), prelude::AckStatus::Ok);
  LOG("app: frame rendered in %lu ms", (unsigned long)(millis() - t0));
}

void handleButton(prelude::ButtonId id) {
  const bool connected = g_state.link == LinkState::Connected;
  switch (prelude::decideButton(id, g_state.buzzer, connected)) {
    case prelude::ButtonAction::Drop:
      return;
    case prelude::ButtonAction::SendButton: {
      uint8_t ev[2];
      prelude::encodeButtonEvent(id, ev);
      ble_link::notifyEvent(ev, sizeof ev);
      return;
    }
    case prelude::ButtonAction::DismissBuzzer: {
      setBuzzer(prelude::BuzzerMode::Off);
      uint8_t ev[1];
      prelude::encodeBuzzerDismissed(ev);
      ble_link::notifyEvent(ev, sizeof ev);
      LOG("app: buzzer dismissed");
      return;
    }
  }
}

void handleLinkUp(const char* peer) {
  if (g_state.link == LinkState::Connected) return;  // repeated auth event on the same link
  LOG("app: link up with %s", peer);
  g_state.link = LinkState::Connected;
  publishSample();
  if (!g_state.serverOwnsScreen) drawPage();
}

void handleLinkDown() {
  LOG("app: link down");
  g_state.link = ble_link::isBonded() ? LinkState::Waiting : LinkState::Pairing;
  setBuzzer(prelude::BuzzerMode::Off);
  ble_link::setRenderBusy(false);
  if (!g_state.serverOwnsScreen) drawPage();
}

void handleHousekeeping(const prelude::SensorSnapshot& s) {
  g_state.sample = s;
  if (s.hasSht4x) g_panelTempC = s.tempCenti / 100.0f;
  publishSample();
  if (!g_state.serverOwnsScreen &&
      prelude::batteryRedrawDue(g_state.drawnBatteryPercent, s.batteryPercent, g_state.drawnAtMs, millis())) {
    drawPage();
  }
}

void appTask(void*) {
  AppMsg m;
  for (;;) {
    if (xQueueReceive(g_queue, &m, portMAX_DELAY) != pdTRUE) continue;
    switch (m.type) {
      case MsgType::Command:      handleCommand(m); break;
      case MsgType::FrameReady:   handleFrameReady(); break;
      case MsgType::Button:       handleButton(static_cast<prelude::ButtonId>(m.button)); break;
      case MsgType::LinkUp:       handleLinkUp(m.peer); break;
      case MsgType::LinkDown:     handleLinkDown(); break;
      case MsgType::Housekeeping: handleHousekeeping(m.sample); break;
    }
  }
}

prelude::SensorSnapshot takeSample() {
  prelude::SensorSnapshot s{};
  s.hasBattery = true;
  s.batteryMv = battery::readMv();
  s.batteryPercent = prelude::batteryPercentFromMv(s.batteryMv);
  s.hasSht4x = g_hasSht4x;
  if (g_hasSht4x) {
    int16_t t;
    uint16_t h;
    if (sensors::readSht4x(t, h)) {
      s.tempCenti = t;
      s.humCenti = h;
    } else {
      s.tempCenti = g_state.sample.tempCenti;
      s.humCenti = g_state.sample.humCenti;
      LOG("sensors: SHT4x read failed, keeping last value");
    }
  }
  LOG("housekeeping: battery %u mV (%u%%) temp %d.%02d C hum %u.%02u %%",
      s.batteryMv, s.batteryPercent, s.tempCenti / 100, abs(s.tempCenti % 100),
      s.humCenti / 100, s.humCenti % 100);
  return s;
}

void housekeepingTask(void*) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(kHousekeepingMs));
    AppMsg m{};
    m.type = MsgType::Housekeeping;
    m.sample = takeSample();
    post(m, pdMS_TO_TICKS(1000));
  }
}

}  // namespace

void begin(bool bondsCleared) {
  g_queue = xQueueCreate(16, sizeof(AppMsg));
  g_frameBuffer = static_cast<uint8_t*>(ps_malloc(prelude::kFrameBytes));
  if (!g_frameBuffer) g_frameBuffer = static_cast<uint8_t*>(malloc(prelude::kFrameBytes));

  battery::begin();
  buzzer::begin();
  g_hasSht4x = sensors::begin();
  display::begin(panelTemp);

  g_state.sample = takeSample();
  if (g_state.sample.hasSht4x) g_panelTempC = g_state.sample.tempCenti / 100.0f;

  ble_link::Callbacks cb{onBleConnected, onBleDisconnected, onBleCommand, onBleFrameReady};
  ble_link::begin(cb, g_frameBuffer, bondsCleared);
  g_state.link = ble_link::isBonded() ? LinkState::Waiting : LinkState::Pairing;
  publishSample();

  buttons::begin(onButton);

  if (bondsCleared) {
    drawPage();
    const char* msg = "Pairing cleared";
    display::drawOverlayBox(msg, strlen(msg));
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
  drawPage();

  xTaskCreatePinnedToCore(appTask, "app", 8192, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(housekeepingTask, "housekeeping", 4096, nullptr, 1, nullptr, 1);
  LOG("app: ready");
}

}  // namespace app
