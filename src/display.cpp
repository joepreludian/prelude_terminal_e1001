#include "display.h"
#include <Arduino.h>
#include <cmath>
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

constexpr int kBoxBorder = 2;
constexpr int kBoxPad = 6;
constexpr int kBoxMidY = kBoxY + kBoxH / 2;  // 22

void refresh(const char* what) {
  const uint32_t t0 = millis();
  g_epaper.update();  // wakes the panel, full refresh, sleeps again
  LOG("display: %s refreshed in %lu ms", what, (unsigned long)(millis() - t0));
}

// ---- glyphs -------------------------------------------------------------

void drawBatteryGlyph(int x, int y, uint8_t percent) {
  // 24x12 body + 2x6 nub; four segments
  g_epaper.drawRect(x, y, 24, 12, TFT_BLACK);
  g_epaper.fillRect(x + 24, y + 3, 2, 6, TFT_BLACK);
  int segments = (percent + 12) / 25;  // 0..4
  for (int i = 0; i < segments; ++i) g_epaper.fillRect(x + 2 + i * 5, y + 2, 4, 8, TFT_BLACK);
}

// 10x16 Bluetooth rune; strike-through when disconnected.
void drawBluetoothGlyph(int x, int y, bool connected) {
  const int sx = x + 4;  // stem
  g_epaper.drawLine(sx, y, sx, y + 15, TFT_BLACK);
  g_epaper.drawLine(sx + 1, y, sx + 1, y + 15, TFT_BLACK);
  g_epaper.drawLine(sx, y, sx + 4, y + 4, TFT_BLACK);        // top-right
  g_epaper.drawLine(sx + 4, y + 4, x, y + 12, TFT_BLACK);    // to bottom-left
  g_epaper.drawLine(sx, y + 15, sx + 4, y + 11, TFT_BLACK);  // bottom-right
  g_epaper.drawLine(sx + 4, y + 11, x, y + 3, TFT_BLACK);    // to top-left
  if (!connected) {
    g_epaper.drawLine(x, y + 15, x + 9, y, TFT_BLACK);
    g_epaper.drawLine(x + 1, y + 15, x + 10, y, TFT_BLACK);
  }
}

// 20x12 semicircular gauge with a needle: lower-left = Saving, lower-right = Performance.
void drawGaugeGlyph(int x, int y, prelude::PowerMode mode) {
  const int cx = x + 10, cy = y + 11, r = 9;
  for (int dx = -r; dx <= r; ++dx) {
    int dy = (int)lroundf(sqrtf((float)(r * r - dx * dx)));
    g_epaper.drawPixel(cx + dx, cy - dy, TFT_BLACK);
  }
  g_epaper.drawLine(cx - r, cy, cx + r, cy, TFT_BLACK);
  const int nx = mode == prelude::PowerMode::Saving ? cx - 6 : cx + 6;
  g_epaper.drawLine(cx, cy, nx, cy - 5, TFT_BLACK);
  g_epaper.drawLine(cx + 1, cy, nx + 1, cy - 5, TFT_BLACK);
  g_epaper.fillCircle(cx, cy, 1, TFT_BLACK);
}

void drawStatusStrip(const char* statusText) {
  g_epaper.drawRect(16, kStripY, kW - 32, kStripH, TFT_BLACK);
  g_epaper.drawRect(17, kStripY + 1, kW - 34, kStripH - 2, TFT_BLACK);
  g_epaper.setTextDatum(ML_DATUM);
  g_epaper.drawString(statusText, 28, kStripY + kStripH / 2, 2);
  g_epaper.setTextDatum(TL_DATUM);
}

}  // namespace

void begin(float (*tempCallback)()) {
  g_epaper.begin();
  if (tempCallback) g_epaper.setTemp(tempCallback);
  g_epaper.setTextColor(TFT_BLACK, TFT_WHITE);
  LOG("display: %d x %d ready", kW, kH);
}

void paintStatusBox(const prelude::StatusBoxState& box) {
  g_epaper.fillRect(kBoxX, kBoxY, kBoxW, kBoxH, TFT_WHITE);
  for (int i = 0; i < kBoxBorder; ++i)
    g_epaper.drawRect(kBoxX + i, kBoxY + i, kBoxW - 2 * i, kBoxH - 2 * i, TFT_BLACK);

  // Left group: bluetooth, gauge
  int x = kBoxX + kBoxBorder + kBoxPad;
  drawBluetoothGlyph(x, kBoxMidY - 8, box.linkUp);
  x += 10 + 6;
  drawGaugeGlyph(x, kBoxMidY - 6, box.powerMode);

  // Right group: battery glyph, percent
  char pct[8];
  snprintf(pct, sizeof pct, "%u%%", box.batteryPercent);
  g_epaper.setTextColor(TFT_BLACK, TFT_WHITE);
  g_epaper.setTextDatum(MR_DATUM);
  const int right = kBoxX + kBoxW - kBoxBorder - kBoxPad;
  g_epaper.drawString(pct, right, kBoxMidY, 2);
  const int textW = g_epaper.textWidth(pct, 2);
  drawBatteryGlyph(right - textW - 4 - 26, kBoxMidY - 6, box.batteryPercent);
  g_epaper.setTextDatum(TL_DATUM);
}

void refreshStatusBox() {
  const uint32_t t0 = millis();
  g_epaper.updataPartial(kBoxX, kBoxY, kBoxW, kBoxH);  // wakes (partial init), refreshes, sleeps
  LOG("display: status box partial in %lu ms", (unsigned long)(millis() - t0));
}

void refreshFull() { refresh("full"); }

void drawPairingPage(const PageInfo& info, const prelude::StatusBoxState& box) {
  g_epaper.fillScreen(TFT_WHITE);
  g_epaper.setTextColor(TFT_BLACK, TFT_WHITE);

  g_epaper.setTextDatum(TL_DATUM);
  g_epaper.drawString("Prelude Terminal", kMargin, 20, 4);
  char line2[48];
  snprintf(line2, sizeof line2, "Seeed Studio e1001 - fw %s", info.fwVersion);
  g_epaper.drawString(line2, kMargin, 56, 2);
  char hint[64];
  snprintf(hint, sizeof hint, "Connect to Bluetooth device \"%s\"", info.deviceName);
  g_epaper.drawString(hint, kMargin, 88, 2);
  g_epaper.drawString("Hold the green button while powering on to unpair", kMargin, 108, 2);

  drawStatusStrip(info.statusText);
  paintStatusBox(box);
  refresh("pairing page");
}

void drawOverlayBox(const char* text, size_t len, const prelude::StatusBoxState& box) {
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
  paintStatusBox(box);
  refresh("overlay");
}

void blitFrame(const uint8_t* packedBlackIsOne, const prelude::StatusBoxState& box) {
  uint8_t* dst = static_cast<uint8_t*>(g_epaper.getPointer());
  for (uint32_t i = 0; i < prelude::kFrameBytes; ++i) dst[i] = (uint8_t)~packedBlackIsOne[i];
  paintStatusBox(box);
  refresh("frame");
}

}  // namespace display
