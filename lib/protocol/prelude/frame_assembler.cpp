#include "prelude/frame_assembler.h"
#include <cstring>
#include "prelude/crc32.h"

namespace prelude {

FrameAssembler::FrameAssembler(uint8_t* buffer, size_t capacity) : buf_(buffer), cap_(capacity) {}

AckStatus FrameAssembler::begin(uint32_t length, uint32_t crc, uint32_t nowMs) {
  if (open_) return AckStatus::Busy;
  if (length != kFrameBytes || length > cap_) return AckStatus::BadArg;
  open_ = true;
  expectedCrc_ = crc;
  received_ = 0;
  openedAtMs_ = nowMs;
  return AckStatus::Ok;
}

bool FrameAssembler::chunk(uint16_t offset, const uint8_t* data, size_t len) {
  if (!open_ || data == nullptr || len == 0) return false;
  if ((size_t)offset + len > kFrameBytes) return false;
  memcpy(buf_ + offset, data, len);
  received_ += (uint32_t)len;
  return true;
}

AckStatus FrameAssembler::end() {
  if (!open_) return AckStatus::BadArg;
  open_ = false;
  if (received_ != kFrameBytes) return AckStatus::Incomplete;
  if (crc32(buf_, kFrameBytes) != expectedCrc_) return AckStatus::CrcMismatch;
  return AckStatus::Ok;
}

void FrameAssembler::abort() { open_ = false; }

bool FrameAssembler::expireIfStale(uint32_t nowMs, uint32_t timeoutMs) {
  if (open_ && (nowMs - openedAtMs_) >= timeoutMs) {
    open_ = false;
    return true;
  }
  return false;
}

}  // namespace prelude
