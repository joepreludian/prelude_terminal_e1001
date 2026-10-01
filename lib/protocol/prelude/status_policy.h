#pragma once
#include <cstdint>
#include "prelude/opcodes.h"

namespace prelude {

constexpr uint32_t kBatteryPartialMinMs = 60000;
constexpr uint16_t kPartialBudget = 20;

struct StatusBoxState {
  bool linkUp;
  PowerMode powerMode;
  uint8_t batteryPercent;  // already rounded to 5
  bool operator==(const StatusBoxState& o) const {
    return linkUp == o.linkUp && powerMode == o.powerMode && batteryPercent == o.batteryPercent;
  }
  bool operator!=(const StatusBoxState& o) const { return !(*this == o); }
};

enum class RefreshKind : uint8_t { None, Partial, Full };

struct RefreshInput {
  StatusBoxState drawn;  // what is on the panel
  StatusBoxState next;   // what should be
  uint32_t nowMs;
  uint32_t lastBatteryPartialMs;
  uint16_t partialsSinceFull;
};

// None: nothing visible changed, or a battery-only change inside the 60 s limiter.
// Partial: link/mode changed, or battery changed and the limiter allows it.
// Full: a Partial would exceed kPartialBudget partials since the last full refresh.
RefreshKind decideRefresh(const RefreshInput& in);
uint8_t roundBatteryPercent(uint8_t raw);  // nearest 5, max 100

}  // namespace prelude
