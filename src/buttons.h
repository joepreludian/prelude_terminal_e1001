#pragma once
#include "prelude/opcodes.h"

namespace buttons {
using PressHandler = void (*)(prelude::ButtonId id);
void begin(PressHandler onPress);
bool isGreenHeld();
}  // namespace buttons
