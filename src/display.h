#pragma once
#include <cstddef>
#include <cstdint>
#include "prelude/status_policy.h"

namespace display {

struct PageInfo {
  const char* fwVersion;
  const char* deviceName;
  const char* statusText;
};

// Status box rectangle (device-owned, drawn over every screen).
// x and width are multiples of 8, as the panel's partial window requires.
constexpr int kBoxX = 672;
constexpr int kBoxY = 8;
constexpr int kBoxW = 120;
constexpr int kBoxH = 28;

void begin(float (*tempCallback)());

// Each of these paints the status box last and then does a full refresh.
void drawPairingPage(const PageInfo& info, const prelude::StatusBoxState& box);
void drawOverlayBox(const char* text, size_t len, const prelude::StatusBoxState& box);
void blitFrame(const uint8_t* packedBlackIsOne, const prelude::StatusBoxState& box);

// Box-only updates.
void paintStatusBox(const prelude::StatusBoxState& box);  // sprite only, no refresh
void refreshStatusBox();                                   // partial refresh of the box rectangle
void refreshFull();                                        // full refresh of the current sprite

}  // namespace display
