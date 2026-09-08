#pragma once
#include <cstdint>

namespace prelude {
// beep 300 ms, silence 300 ms, repeat. toggle() is called every kStepMs.
class BuzzerPattern {
 public:
  static constexpr uint32_t kStepMs = 300;
  void start() { running_ = true; toneOn_ = true; }
  void stop() { running_ = false; toneOn_ = false; }
  void toggle() { if (running_) toneOn_ = !toneOn_; }
  bool running() const { return running_; }
  bool toneOn() const { return toneOn_; }
 private:
  bool running_ = false;
  bool toneOn_ = false;
};
}  // namespace prelude
