#pragma once
#include <cstdint>
#include "prelude/opcodes.h"

namespace power {
void begin();                        // applies Performance
void set(prelude::PowerMode mode);   // CPU clock + BLE conn params; no-op when unchanged
prelude::PowerMode mode();
uint32_t housekeepingMs();           // 30 s Performance, 120 s Saving
}  // namespace power
