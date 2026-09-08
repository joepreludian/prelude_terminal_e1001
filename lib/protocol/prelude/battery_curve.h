#pragma once
#include <cstdint>

namespace prelude {
uint8_t batteryPercentFromMv(uint16_t mv);
// Redraw the pairing page when the level moved 5 % or more, or moved at all
// after 5 minutes.
bool batteryRedrawDue(uint8_t drawnPercent, uint8_t nowPercent, uint32_t drawnAtMs, uint32_t nowMs);
}
