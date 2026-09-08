#include "display.h"
#include <Arduino.h>
#include <string>
#include "TFT_eSPI.h"
#include "log.h"
#include "prelude/opcodes.h"
#include "prelude/word_wrap.h"

namespace display {
namespace {

// Allocates the 48,000-byte 1-bit sprite in its constructor (needs PSRAM flags).
EPaper g_epaper;

constexpr int kW = prelude::kScreenWidth;
constexpr int kH = prelude::kScreenHeight;
constexpr int kMargin = 24;
constexpr int kStripH = 36;
constexpr int kStripY = kH - kStripH - 16;

void refresh(const char* what) {
  const uint32_t t0 = millis();
  g_epaper.update();  // wakes the panel, full refresh, sleeps again
  LOG("display: %s refreshed in %lu ms", what, (unsigned long)(millis() - t0));
}

void drawBatteryGlyph(int x, int y, uint8_t percent) {
  // 24x12 body + 2x6 nub; four segments
  g_epaper.drawRect(x, y, 24, 12, TFT_BLACK);
  g_epaper.fillRect(x + 24, y + 3, 2, 6, TFT_BLACK);
  int segments = (percent + 12) / 25;  // 0..4
  for (int i = 0; i < segments; ++i) g_epaper.fillRect(x + 2 + i * 5, y + 2, 4, 8, TFT_BLACK);
}

void drawStatusStrip(const char* statusText, uint8_t batteryPercent) {
  g_epaper.drawRect(16, kStripY, kW - 32, kStripH, TFT_BLACK);
  g_epaper.drawRect(17, kStripY + 1, kW - 34, kStripH - 2, TFT_BLACK);
  g_epaper.setTextDatum(ML_DATUM);
  g_epaper.drawString(statusText, 28, kStripY + kStripH / 2, 2);
  char pct[8];
  snprintf(pct, sizeof pct, "%u%%", batteryPercent);
  g_epaper.setTextDatum(MR_DATUM);
  g_epaper.drawString(pct, kW - 28 - 30, kStripY + kStripH / 2, 2);
  drawBatteryGlyph(kW - 28 - 26, kStripY + kStripH / 2 - 6, batteryPercent);
  g_epaper.setTextDatum(TL_DATUM);
}

}  // namespace

void begin(float (*tempCallback)()) {
  g_epaper.begin();
  if (tempCallback) g_epaper.setTemp(tempCallback);
  g_epaper.setTextColor(TFT_BLACK, TFT_WHITE);
  LOG("display: %d x %d ready", kW, kH);
}

void drawPairingPage(const PageInfo& info) {
  g_epaper.fillScreen(TFT_WHITE);
  g_epaper.setTextColor(TFT_BLACK, TFT_WHITE);

  g_epaper.setTextDatum(TL_DATUM);
  g_epaper.drawString("Prelude Terminal", kMargin, 20, 4);
  char fw[24];
  snprintf(fw, sizeof fw, "[fw %s]", info.fwVersion);
  g_epaper.setTextDatum(TR_DATUM);
  g_epaper.drawString(fw, kW - kMargin, 28, 2);

  g_epaper.setTextDatum(TL_DATUM);
  g_epaper.drawString("Seeed Studio e1001", kMargin, 56, 2);
  char hint[64];
  snprintf(hint, sizeof hint, "Connect to Bluetooth device \"%s\"", info.deviceName);
  g_epaper.drawString(hint, kMargin, 88, 2);
  g_epaper.drawString("Hold the green button while powering on to unpair", kMargin, 108, 2);

  drawStatusStrip(info.statusText, info.batteryPercent);
  refresh("pairing page");
}

void drawOverlayBox(const char* text, size_t len) {
  constexpr int boxW = 560, boxH = 240, pad = 12, border = 2, lineH = 30;
  constexpr int x0 = (kW - boxW) / 2, y0 = (kH - boxH) / 2;
  constexpr size_t maxLines = 6;

  g_epaper.fillRect(x0, y0, boxW, boxH, TFT_WHITE);
  for (int i = 0; i < border; ++i) g_epaper.drawRect(x0 + i, y0 + i, boxW - 2 * i, boxH - 2 * i, TFT_BLACK);

  const std::string msg(text, len);
  auto lines = prelude::wrapText(
      msg, boxW - 2 * (pad + border),
      [](const std::string& s) { return (int)g_epaper.textWidth(s.c_str(), 4); }, maxLines);

  g_epaper.setTextColor(TFT_BLACK, TFT_WHITE);
  g_epaper.setTextDatum(TL_DATUM);
  int y = y0 + border + pad;
  for (const auto& line : lines) {
    g_epaper.drawString(line.c_str(), x0 + border + pad, y, 4);
    y += lineH;
  }
  refresh("overlay");
}

void blitFrame(const uint8_t* packedBlackIsOne) {
  uint8_t* dst = static_cast<uint8_t*>(g_epaper.getPointer());
  for (uint32_t i = 0; i < prelude::kFrameBytes; ++i) dst[i] = (uint8_t)~packedBlackIsOne[i];
  refresh("frame");
}

}  // namespace display
