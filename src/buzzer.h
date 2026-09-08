#pragma once
#include "prelude/button_policy.h"

namespace buzzer {
void begin();
void setMode(prelude::BuzzerMode mode);  // call from the app task only
prelude::BuzzerMode mode();
}  // namespace buzzer
