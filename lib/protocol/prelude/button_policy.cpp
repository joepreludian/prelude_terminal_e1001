#include "prelude/button_policy.h"

namespace prelude {
ButtonAction decideButton(ButtonId id, BuzzerMode mode, bool connected) {
  if (!connected) return ButtonAction::Drop;
  if (mode == BuzzerMode::Dismissable && id == ButtonId::Green) return ButtonAction::DismissBuzzer;
  return ButtonAction::SendButton;
}
}  // namespace prelude
