#include <unity.h>
#include <string>
#include "prelude/sensors_tlv.h"
#include "prelude/sht4x_codec.h"
#include "prelude/word_wrap.h"

using namespace prelude;

void setUp() {}
void tearDown() {}

void test_tlv_all_sensors() {
  SensorSnapshot s{true, -1234, 5650, true, 87, 3950};
  uint8_t out[kMaxSensorTlv];
  size_t n = encodeSensorTlv(s, out, sizeof out);
  const uint8_t expected[] = {0x01, 2, 0x2E, 0xFB, 0x02, 2, 0x12, 0x16, 0x03, 3, 87, 0x6E, 0x0F};
  TEST_ASSERT_EQUAL_size_t(13, n);
  TEST_ASSERT_EQUAL_MEMORY(expected, out, 13);
}

void test_tlv_omits_missing_sensors() {
  SensorSnapshot s{false, 0, 0, true, 50, 3700};
  uint8_t out[kMaxSensorTlv];
  size_t n = encodeSensorTlv(s, out, sizeof out);
  TEST_ASSERT_EQUAL_size_t(5, n);
  TEST_ASSERT_EQUAL_UINT8(0x03, out[0]);
  TEST_ASSERT_EQUAL_size_t(0, encodeSensorTlv(s, out, 4));  // too small -> 0
}

void test_sht4x_crc_vector() {
  const uint8_t d[] = {0xBE, 0xEF};
  TEST_ASSERT_EQUAL_HEX8(0x92, sht4xCrc8(d, 2));
}

void test_sht4x_decode() {
  // temp raw 0x6666 -> 25.00 C ; hum raw 0x8000 -> 56.50 %RH
  uint8_t raw[6] = {0x66, 0x66, 0, 0x80, 0x00, 0};
  raw[2] = sht4xCrc8(raw, 2);
  raw[5] = sht4xCrc8(raw + 3, 2);
  int16_t t; uint16_t h;
  TEST_ASSERT_TRUE(decodeSht4x(raw, t, h));
  TEST_ASSERT_EQUAL_INT16(2500, t);
  TEST_ASSERT_EQUAL_UINT16(5650, h);
  raw[2] ^= 0xFF;
  TEST_ASSERT_FALSE(decodeSht4x(raw, t, h));
}

void test_sht4x_humidity_clamped() {
  uint8_t raw[6] = {0x00, 0x00, 0, 0x00, 0x00, 0};
  raw[2] = sht4xCrc8(raw, 2);
  raw[5] = sht4xCrc8(raw + 3, 2);
  int16_t t; uint16_t h;
  TEST_ASSERT_TRUE(decodeSht4x(raw, t, h));
  TEST_ASSERT_EQUAL_INT16(-4500, t);
  TEST_ASSERT_EQUAL_UINT16(0, h);
}

static int measure10(const std::string& s) { return (int)s.size() * 10; }

void test_wrap_basic() {
  auto lines = wrapText("hello big world", 100, measure10, 6);
  TEST_ASSERT_EQUAL_size_t(2, lines.size());
  TEST_ASSERT_EQUAL_STRING("hello big", lines[0].c_str());
  TEST_ASSERT_EQUAL_STRING("world", lines[1].c_str());
}

void test_wrap_newline_and_long_word() {
  auto lines = wrapText("ab\ncd", 100, measure10, 6);
  TEST_ASSERT_EQUAL_size_t(2, lines.size());
  TEST_ASSERT_EQUAL_STRING("ab", lines[0].c_str());
  TEST_ASSERT_EQUAL_STRING("cd", lines[1].c_str());

  auto broken = wrapText("abcdefghijkl", 50, measure10, 6);
  TEST_ASSERT_EQUAL_size_t(3, broken.size());
  TEST_ASSERT_EQUAL_STRING("abcde", broken[0].c_str());
  TEST_ASSERT_EQUAL_STRING("fghij", broken[1].c_str());
  TEST_ASSERT_EQUAL_STRING("kl", broken[2].c_str());
}

void test_wrap_max_lines_and_empty() {
  auto lines = wrapText("a b c d e", 10, measure10, 2);
  TEST_ASSERT_EQUAL_size_t(2, lines.size());
  TEST_ASSERT_EQUAL_size_t(0, wrapText("", 100, measure10, 6).size());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_tlv_all_sensors);
  RUN_TEST(test_tlv_omits_missing_sensors);
  RUN_TEST(test_sht4x_crc_vector);
  RUN_TEST(test_sht4x_decode);
  RUN_TEST(test_sht4x_humidity_clamped);
  RUN_TEST(test_wrap_basic);
  RUN_TEST(test_wrap_newline_and_long_word);
  RUN_TEST(test_wrap_max_lines_and_empty);
  return UNITY_END();
}
