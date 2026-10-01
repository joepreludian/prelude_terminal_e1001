#include "power.h"
#include <Arduino.h>
#include "ble_link.h"
#include "log.h"

namespace power {
namespace {

prelude::PowerMode g_mode = prelude::PowerMode::Performance;
bool g_applied = false;

struct Profile {
  uint32_t cpuMhz;
  uint32_t housekeepingMs;
  ble_link::ConnParams conn;
};

constexpr Profile kPerformance{240, 30000, {12, 24, 0, 400}};  // 15-30 ms, latency 0, 4 s
constexpr Profile kSaving{80, 120000, {80, 160, 4, 600}};      // 100-200 ms, latency 4, 6 s

const Profile& profile(prelude::PowerMode m) {
  return m == prelude::PowerMode::Saving ? kSaving : kPerformance;
}

void apply(prelude::PowerMode m) {
  const Profile& p = profile(m);
  setCpuFrequencyMhz(p.cpuMhz);
  ble_link::setConnParams(p.conn);
  LOG("power: %s (cpu %lu MHz, housekeeping %lu s)",
      m == prelude::PowerMode::Saving ? "saving" : "performance",
      (unsigned long)getCpuFrequencyMhz(), (unsigned long)(p.housekeepingMs / 1000));
}

}  // namespace

void begin() {
  g_mode = prelude::PowerMode::Performance;
  apply(g_mode);
  g_applied = true;
}

void set(prelude::PowerMode mode) {
  if (g_applied && mode == g_mode) return;
  g_mode = mode;
  apply(mode);
  g_applied = true;
}

prelude::PowerMode mode() { return g_mode; }
uint32_t housekeepingMs() { return profile(g_mode).housekeepingMs; }

}  // namespace power
