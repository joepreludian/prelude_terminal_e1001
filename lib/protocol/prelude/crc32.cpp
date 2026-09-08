#include "prelude/crc32.h"

namespace prelude {
namespace {

struct Table {
  uint32_t t[256];
  constexpr Table() : t() {
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
      t[i] = c;
    }
  }
};
constexpr Table kTable{};

}  // namespace

uint32_t crc32(const uint8_t* data, size_t len) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; ++i) c = kTable.t[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
  return c ^ 0xFFFFFFFFu;
}

}  // namespace prelude
