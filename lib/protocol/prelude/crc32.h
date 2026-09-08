#pragma once
#include <cstddef>
#include <cstdint>

namespace prelude {
// IEEE 802.3 CRC-32 (same as zlib.crc32). Returns 0 for empty input.
uint32_t crc32(const uint8_t* data, size_t len);
}
