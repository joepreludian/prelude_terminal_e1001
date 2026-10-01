#include <unity.h>
#include "prelude/status_policy.h"

using namespace prelude;

void setUp() {}
void tearDown() {}

static StatusBoxState box(bool up, PowerMode m, uint8_t pct) { return StatusBoxState{up, m, pct}; }
static RefreshInput in(StatusBoxState drawn, StatusBoxState next, uint32_t now = 0,
                       uint32_t lastBat = 0, uint16_t partials = 0) {
  return RefreshInput{drawn, next, now, lastBat, partials};
}

void test_round_battery_percent() {
  TEST_ASSERT_EQUAL_UINT8(0, roundBatteryPercent(0));
  TEST_ASSERT_EQUAL_UINT8(0, roundBatteryPercent(2));
  TEST_ASSERT_EQUAL_UINT8(5, roundBatteryPercent(3));
  TEST_ASSERT_EQUAL_UINT8(85, roundBatteryPercent(87));
  TEST_ASSERT_EQUAL_UINT8(95, roundBatteryPercent(97));
  TEST_ASSERT_EQUAL_UINT8(100, roundBatteryPercent(98));
  TEST_ASSERT_EQUAL_UINT8(100, roundBatteryPercent(100));
}

void test_identical_state_is_none() {
  auto s = box(true, PowerMode::Performance, 85);
  TEST_ASSERT_EQUAL(RefreshKind::None, decideRefresh(in(s, s, 999999, 0, 19)));
}

void test_link_change_is_immediate_partial() {
  auto a = box(true, PowerMode::Performance, 85);
  auto b = box(false, PowerMode::Performance, 85);
  TEST_ASSERT_EQUAL(RefreshKind::Partial, decideRefresh(in(a, b, 1000, 1000, 0)));
}

void test_mode_change_is_immediate_partial() {
  auto a = box(true, PowerMode::Performance, 85);
  auto b = box(true, PowerMode::Saving, 85);
  TEST_ASSERT_EQUAL(RefreshKind::Partial, decideRefresh(in(a, b, 1000, 1000, 0)));
}

void test_battery_change_respects_60s_limiter() {
  auto a = box(true, PowerMode::Performance, 85);
  auto b = box(true, PowerMode::Performance, 80);
  TEST_ASSERT_EQUAL(RefreshKind::None, decideRefresh(in(a, b, 50000, 0, 0)));
  TEST_ASSERT_EQUAL(RefreshKind::None, decideRefresh(in(a, b, 59999, 0, 0)));
  TEST_ASSERT_EQUAL(RefreshKind::Partial, decideRefresh(in(a, b, 60000, 0, 0)));
  TEST_ASSERT_EQUAL(RefreshKind::Partial, decideRefresh(in(a, b, 120000, 30000, 0)));
}

void test_battery_limiter_survives_millis_wrap() {
  auto a = box(true, PowerMode::Performance, 85);
  auto b = box(true, PowerMode::Performance, 80);
  // last partial just before wrap, now just after: 70 s elapsed in unsigned arithmetic
  TEST_ASSERT_EQUAL(RefreshKind::Partial, decideRefresh(in(a, b, 10000u, 0xFFFFFFFFu - 60000u + 1u, 0)));
  // 30 s elapsed across the wrap: still too soon
  TEST_ASSERT_EQUAL(RefreshKind::None, decideRefresh(in(a, b, 10000u, 0xFFFFFFFFu - 20000u + 1u, 0)));
}

void test_budget_upgrades_partial_to_full() {
  auto a = box(true, PowerMode::Performance, 85);
  auto b = box(false, PowerMode::Performance, 85);
  TEST_ASSERT_EQUAL(RefreshKind::Partial, decideRefresh(in(a, b, 0, 0, 19)));  // this would be the 20th
  TEST_ASSERT_EQUAL(RefreshKind::Full, decideRefresh(in(a, b, 0, 0, 20)));     // the 21st becomes full
  TEST_ASSERT_EQUAL(RefreshKind::Full, decideRefresh(in(a, b, 0, 0, 500)));
}

void test_budget_does_not_force_full_when_nothing_changed() {
  auto s = box(true, PowerMode::Performance, 85);
  TEST_ASSERT_EQUAL(RefreshKind::None, decideRefresh(in(s, s, 0, 0, 20)));
}

void test_battery_change_blocked_by_limiter_is_not_upgraded() {
  auto a = box(true, PowerMode::Performance, 85);
  auto b = box(true, PowerMode::Performance, 80);
  TEST_ASSERT_EQUAL(RefreshKind::None, decideRefresh(in(a, b, 1000, 0, 20)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_round_battery_percent);
  RUN_TEST(test_identical_state_is_none);
  RUN_TEST(test_link_change_is_immediate_partial);
  RUN_TEST(test_mode_change_is_immediate_partial);
  RUN_TEST(test_battery_change_respects_60s_limiter);
  RUN_TEST(test_battery_limiter_survives_millis_wrap);
  RUN_TEST(test_budget_upgrades_partial_to_full);
  RUN_TEST(test_budget_does_not_force_full_when_nothing_changed);
  RUN_TEST(test_battery_change_blocked_by_limiter_is_not_upgraded);
  return UNITY_END();
}
