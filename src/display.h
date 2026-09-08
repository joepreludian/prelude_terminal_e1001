#pragma once
#include <cstddef>
#include <cstdint>

namespace display {

struct PageInfo {
  const char* fwVersion;
  const char* deviceName;
  const char* statusText;
  uint8_t batteryPercent;
};

void begin(float (*tempCallback)());
void drawPairingPage(const PageInfo& info);
void drawOverlayBox(const char* text, size_t len);
void blitFrame(const uint8_t* packedBlackIsOne);

}  // namespace display
