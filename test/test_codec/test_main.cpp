#include <unity.h>
#include <cstring>
#include "prelude/codec.h"
#include "prelude/crc32.h"

using namespace prelude;

void setUp() {}
void tearDown() {}

void test_crc32_known_vector() {
  const char* s = "123456789";
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, crc32(reinterpret_cast<const uint8_t*>(s), 9));
}

void test_crc32_empty_is_zero() {
  TEST_ASSERT_EQUAL_HEX32(0u, crc32(nullptr, 0));
}

void test_parse_display_status() {
  uint8_t buf[] = {0x01, 'h', 'i'};
  ParsedCommand c = parseCommand(buf, sizeof buf);
  TEST_ASSERT_TRUE(c.valid);
  TEST_ASSERT_EQUAL_UINT8(0x01, c.opcode);
  TEST_ASSERT_EQUAL_UINT16(2, c.textLen);
  TEST_ASSERT_EQUAL_MEMORY("hi", c.text, 2);
}

void test_parse_display_status_rejects_empty_and_too_long() {
  uint8_t empty[] = {0x01};
  TEST_ASSERT_FALSE(parseCommand(empty, 1).valid);
  uint8_t big[1 + kMaxStatusText + 1];
  memset(big, 'x', sizeof big);
  big[0] = 0x01;
  TEST_ASSERT_FALSE(parseCommand(big, sizeof big).valid);
  TEST_ASSERT_TRUE(parseCommand(big, 1 + kMaxStatusText).valid);
}

void test_parse_frame_begin() {
  uint8_t buf[] = {0x02, 0x80, 0xBB, 0x00, 0x00, 0x78, 0x56, 0x34, 0x12};
  ParsedCommand c = parseCommand(buf, sizeof buf);
  TEST_ASSERT_TRUE(c.valid);
  TEST_ASSERT_EQUAL_UINT32(48000u, c.frameLength);
  TEST_ASSERT_EQUAL_HEX32(0x12345678u, c.frameCrc);
}

void test_parse_frame_begin_wrong_length_is_invalid() {
  uint8_t buf[] = {0x02, 0x80, 0xBB, 0x00, 0x00};
  TEST_ASSERT_FALSE(parseCommand(buf, sizeof buf).valid);
}

void test_parse_no_payload_commands() {
  const uint8_t ops[] = {0x03, 0x10, 0x11, 0x12};
  for (uint8_t op : ops) {
    ParsedCommand c = parseCommand(&op, 1);
    TEST_ASSERT_TRUE(c.valid);
    TEST_ASSERT_EQUAL_UINT8(op, c.opcode);
    uint8_t withJunk[] = {op, 0x00};
    TEST_ASSERT_FALSE(parseCommand(withJunk, 2).valid);
  }
}

void test_parse_unknown_opcode_and_empty() {
  uint8_t buf[] = {0x7F};
  ParsedCommand c = parseCommand(buf, 1);
  TEST_ASSERT_FALSE(c.valid);
  TEST_ASSERT_EQUAL_UINT8(0x7F, c.opcode);
  ParsedCommand e = parseCommand(buf, 0);
  TEST_ASSERT_FALSE(e.valid);
  TEST_ASSERT_EQUAL_UINT8(0, e.opcode);
}

void test_parse_frame_chunk() {
  uint8_t buf[] = {0x34, 0x12, 0xAA, 0xBB, 0xCC};
  FrameChunk ch = parseFrameChunk(buf, sizeof buf);
  TEST_ASSERT_TRUE(ch.valid);
  TEST_ASSERT_EQUAL_UINT16(0x1234, ch.offset);
  TEST_ASSERT_EQUAL_size_t(3, ch.len);
  TEST_ASSERT_EQUAL_UINT8(0xAA, ch.data[0]);
  TEST_ASSERT_FALSE(parseFrameChunk(buf, 2).valid);
}

void test_encode_events() {
  uint8_t ack[3];
  TEST_ASSERT_EQUAL_size_t(3, encodeAck(0x03, AckStatus::CrcMismatch, ack));
  TEST_ASSERT_EQUAL_UINT8(0x03, ack[0]);
  TEST_ASSERT_EQUAL_UINT8(0x03, ack[1]);
  TEST_ASSERT_EQUAL_UINT8(3, ack[2]);

  uint8_t btn[2];
  TEST_ASSERT_EQUAL_size_t(2, encodeButtonEvent(ButtonId::Green, btn));
  TEST_ASSERT_EQUAL_UINT8(0x01, btn[0]);
  TEST_ASSERT_EQUAL_UINT8(2, btn[1]);

  uint8_t dis[1];
  TEST_ASSERT_EQUAL_size_t(1, encodeBuzzerDismissed(dis));
  TEST_ASSERT_EQUAL_UINT8(0x02, dis[0]);
}

void test_parse_set_power_mode() {
  uint8_t saving[] = {0x20, 0x00};
  ParsedCommand c = parseCommand(saving, 2);
  TEST_ASSERT_TRUE(c.valid);
  TEST_ASSERT_EQUAL_UINT8(0x20, c.opcode);
  TEST_ASSERT_EQUAL(PowerMode::Saving, c.powerMode);

  uint8_t perf[] = {0x20, 0x01};
  c = parseCommand(perf, 2);
  TEST_ASSERT_TRUE(c.valid);
  TEST_ASSERT_EQUAL(PowerMode::Performance, c.powerMode);
}

void test_parse_set_power_mode_rejects_bad_payload() {
  uint8_t none[] = {0x20};
  TEST_ASSERT_FALSE(parseCommand(none, 1).valid);
  uint8_t two[] = {0x20, 0x02};
  TEST_ASSERT_FALSE(parseCommand(two, 2).valid);
  uint8_t extra[] = {0x20, 0x01, 0x00};
  TEST_ASSERT_FALSE(parseCommand(extra, 3).valid);
}

void test_encode_info() {
  DeviceInfo info{2, 0, 2, 0, 800, 480, 87, 3950, PowerMode::Saving};
  uint8_t out[kInfoSize];
  TEST_ASSERT_EQUAL_size_t(12, encodeInfo(info, out));
  const uint8_t expected[] = {2, 0, 2, 0, 0x20, 0x03, 0xE0, 0x01, 87, 0x6E, 0x0F, 0};
  TEST_ASSERT_EQUAL_MEMORY(expected, out, 12);
  info.powerMode = PowerMode::Performance;
  encodeInfo(info, out);
  TEST_ASSERT_EQUAL_UINT8(1, out[11]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_crc32_known_vector);
  RUN_TEST(test_crc32_empty_is_zero);
  RUN_TEST(test_parse_display_status);
  RUN_TEST(test_parse_display_status_rejects_empty_and_too_long);
  RUN_TEST(test_parse_frame_begin);
  RUN_TEST(test_parse_frame_begin_wrong_length_is_invalid);
  RUN_TEST(test_parse_no_payload_commands);
  RUN_TEST(test_parse_unknown_opcode_and_empty);
  RUN_TEST(test_parse_set_power_mode);
  RUN_TEST(test_parse_set_power_mode_rejects_bad_payload);
  RUN_TEST(test_parse_frame_chunk);
  RUN_TEST(test_encode_events);
  RUN_TEST(test_encode_info);
  return UNITY_END();
}
