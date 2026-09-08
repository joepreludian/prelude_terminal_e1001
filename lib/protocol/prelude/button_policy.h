#pragma once
#include "prelude/opcodes.h"

namespace prelude {
enum class BuzzerMode : uint8_t { Off, On, Dismissable };
enum class ButtonAction : uint8_t { Drop, SendButton, DismissBuzzer };
ButtonAction decideButton(ButtonId id, BuzzerMode mode, bool connected);
}
