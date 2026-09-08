#include <unity.h>
#include <algorithm>
#include <vector>
#include "prelude/frame_assembler.h"
#include "prelude/crc32.h"

using namespace prelude;

static std::vector<uint8_t> g_buf(kFrameBytes);
static std::vector<uint8_t> g_img(kFrameBytes);

void setUp() {
  for (size_t i = 0; i < kFrameBytes; ++i) g_img[i] = (uint8_t)(i * 7);
  std::fill(g_buf.begin(), g_buf.end(), 0);
}
void tearDown() {}

static void sendAll(FrameAssembler& fa, size_t chunk) {
  for (size_t off = 0; off < kFrameBytes; off += chunk) {
    size_t n = std::min(chunk, (size_t)kFrameBytes - off);
    TEST_ASSERT_TRUE(fa.chunk((uint16_t)off, g_img.data() + off, n));
  }
}

void test_full_transfer_ok() {
  FrameAssembler fa(g_buf.data(), g_buf.size());
  TEST_ASSERT_EQUAL(AckStatus::Ok, fa.begin(kFrameBytes, crc32(g_img.data(), kFrameBytes), 1000));
  TEST_ASSERT_TRUE(fa.isOpen());
  sendAll(fa, 512);
  TEST_ASSERT_EQUAL(AckStatus::Ok, fa.end());
  TEST_ASSERT_FALSE(fa.isOpen());
  TEST_ASSERT_EQUAL_MEMORY(g_img.data(), fa.data(), kFrameBytes);
}

void test_out_of_order_chunks_ok() {
  FrameAssembler fa(g_buf.data(), g_buf.size());
  fa.begin(kFrameBytes, crc32(g_img.data(), kFrameBytes), 0);
  const size_t chunk = 244;
  std::vector<size_t> offsets;
  for (size_t off = 0; off < kFrameBytes; off += chunk) offsets.push_back(off);
  for (auto it = offsets.rbegin(); it != offsets.rend(); ++it) {
    size_t n = std::min(chunk, (size_t)kFrameBytes - *it);
    fa.chunk((uint16_t)*it, g_img.data() + *it, n);
  }
  TEST_ASSERT_EQUAL(AckStatus::Ok, fa.end());
}

void test_begin_rejects_bad_length_and_busy() {
  FrameAssembler fa(g_buf.data(), g_buf.size());
  TEST_ASSERT_EQUAL(AckStatus::BadArg, fa.begin(100, 0, 0));
  TEST_ASSERT_EQUAL(AckStatus::Ok, fa.begin(kFrameBytes, 0, 0));
  TEST_ASSERT_EQUAL(AckStatus::Busy, fa.begin(kFrameBytes, 0, 0));
}

void test_incomplete_and_crc_mismatch() {
  FrameAssembler fa(g_buf.data(), g_buf.size());
  fa.begin(kFrameBytes, crc32(g_img.data(), kFrameBytes), 0);
  fa.chunk(0, g_img.data(), 512);
  TEST_ASSERT_EQUAL(AckStatus::Incomplete, fa.end());
  TEST_ASSERT_FALSE(fa.isOpen());

  fa.begin(kFrameBytes, 0xDEADBEEF, 0);
  sendAll(fa, 512);
  TEST_ASSERT_EQUAL(AckStatus::CrcMismatch, fa.end());
}

void test_chunk_rejected_when_closed_or_out_of_range() {
  FrameAssembler fa(g_buf.data(), g_buf.size());
  TEST_ASSERT_FALSE(fa.chunk(0, g_img.data(), 10));
  fa.begin(kFrameBytes, 0, 0);
  TEST_ASSERT_FALSE(fa.chunk((uint16_t)(kFrameBytes - 4), g_img.data(), 8));
  TEST_ASSERT_FALSE(fa.chunk(0, g_img.data(), 0));
  TEST_ASSERT_EQUAL_UINT32(0, fa.received());
}

void test_end_without_begin_is_bad_arg() {
  FrameAssembler fa(g_buf.data(), g_buf.size());
  TEST_ASSERT_EQUAL(AckStatus::BadArg, fa.end());
}

void test_stale_frame_expires() {
  FrameAssembler fa(g_buf.data(), g_buf.size());
  fa.begin(kFrameBytes, 0, 1000);
  TEST_ASSERT_FALSE(fa.expireIfStale(5000));
  TEST_ASSERT_TRUE(fa.isOpen());
  TEST_ASSERT_TRUE(fa.expireIfStale(11000));
  TEST_ASSERT_FALSE(fa.isOpen());
  TEST_ASSERT_EQUAL(AckStatus::Ok, fa.begin(kFrameBytes, 0, 11000));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_full_transfer_ok);
  RUN_TEST(test_out_of_order_chunks_ok);
  RUN_TEST(test_begin_rejects_bad_length_and_busy);
  RUN_TEST(test_incomplete_and_crc_mismatch);
  RUN_TEST(test_chunk_rejected_when_closed_or_out_of_range);
  RUN_TEST(test_end_without_begin_is_bad_arg);
  RUN_TEST(test_stale_frame_expires);
  return UNITY_END();
}
