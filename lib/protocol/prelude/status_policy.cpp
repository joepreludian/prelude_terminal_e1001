#include "prelude/status_policy.h"

namespace prelude {

uint8_t roundBatteryPercent(uint8_t raw) {
  unsigned r = ((unsigned)raw + 2u) / 5u * 5u;
  return (uint8_t)(r > 100u ? 100u : r);
}

RefreshKind decideRefresh(const RefreshInput& in) {
  if (in.next == in.drawn) return RefreshKind::None;
  const bool linkOrMode =
      in.next.linkUp != in.drawn.linkUp || in.next.powerMode != in.drawn.powerMode;
  if (!linkOrMode) {
    // battery-only change; unsigned subtraction survives millis() wrap
    if ((uint32_t)(in.nowMs - in.lastBatteryPartialMs) < kBatteryPartialMinMs) return RefreshKind::None;
  }
  return in.partialsSinceFull >= kPartialBudget ? RefreshKind::Full : RefreshKind::Partial;
}

}  // namespace prelude
