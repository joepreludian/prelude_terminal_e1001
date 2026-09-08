#include "prelude/battery_curve.h"
#include <cstddef>

namespace prelude {
namespace {
struct Point { uint16_t mv; uint8_t pct; };
constexpr Point kCurve[] = {
    {3300, 0}, {3600, 10}, {3700, 25}, {3750, 40}, {3800, 55},
    {3850, 65}, {3900, 75}, {4000, 85}, {4100, 95}, {4200, 100},
};
constexpr size_t kPoints = sizeof(kCurve) / sizeof(kCurve[0]);
}  // namespace

uint8_t batteryPercentFromMv(uint16_t mv) {
  if (mv <= kCurve[0].mv) return kCurve[0].pct;
  if (mv >= kCurve[kPoints - 1].mv) return kCurve[kPoints - 1].pct;
  for (size_t i = 1; i < kPoints; ++i) {
    if (mv <= kCurve[i].mv) {
      const Point& a = kCurve[i - 1];
      const Point& b = kCurve[i];
      uint32_t span = b.mv - a.mv;
      uint32_t pos = mv - a.mv;
      return (uint8_t)(a.pct + ((b.pct - a.pct) * pos + span / 2) / span);
    }
  }
  return 100;
}

bool batteryRedrawDue(uint8_t drawnPercent, uint8_t nowPercent, uint32_t drawnAtMs, uint32_t nowMs) {
  int diff = (int)nowPercent - (int)drawnPercent;
  if (diff < 0) diff = -diff;
  if (diff == 0) return false;
  if (diff >= 5) return true;
  return (nowMs - drawnAtMs) >= 300000u;
}

}  // namespace prelude
