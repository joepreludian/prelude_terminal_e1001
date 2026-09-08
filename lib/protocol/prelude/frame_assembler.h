#pragma once
#include <cstddef>
#include <cstdint>
#include "prelude/opcodes.h"

namespace prelude {

// Collects a 48,000-byte 1-bit frame from offset-addressed chunks.
// The caller owns the buffer (it lives in PSRAM on the device).
class FrameAssembler {
 public:
  FrameAssembler(uint8_t* buffer, size_t capacity);

  AckStatus begin(uint32_t length, uint32_t crc, uint32_t nowMs);
  bool chunk(uint16_t offset, const uint8_t* data, size_t len);
  AckStatus end();
  void abort();
  bool isOpen() const { return open_; }
  bool expireIfStale(uint32_t nowMs, uint32_t timeoutMs = 10000);
  const uint8_t* data() const { return buf_; }
  uint32_t received() const { return received_; }

 private:
  uint8_t* buf_;
  size_t cap_;
  bool open_ = false;
  uint32_t expectedCrc_ = 0;
  uint32_t received_ = 0;
  uint32_t openedAtMs_ = 0;
};

}  // namespace prelude
