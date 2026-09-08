#pragma once
#include <cstdint>

namespace battery {
void begin();
uint16_t readMv();  // 8-sample average, divider compensated
}  // namespace battery
