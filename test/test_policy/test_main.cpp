#include <unity.h>
#include "prelude/battery_curve.h"
#include "prelude/button_policy.h"
#include "prelude/buzzer_pattern.h"

using namespace prelude;

void setUp() {}
void tearDown() {}

void test_battery_curve_edges() {
  TEST_ASSERT_EQUAL_UINT8(0, batteryPercentFromMv(3000));
  TEST_ASSERT_EQUAL_UINT8(0, batteryPercentFromMv(3300));
  TEST_ASSERT_EQUAL_UINT8(100, batteryPercentFromMv(4200));
  TEST_ASSERT_EQUAL_UINT8(100, batteryPercentFromMv(4300));
}

void test_battery_curve_points_and_interpolation() {
  TEST_ASSERT_EQUAL_UINT8(10, batteryPercentFromMv(3600));
  TEST_ASSERT_EQUAL_UINT8(55, batteryPercentFromMv(3800));
  TEST_ASSERT_EQUAL_UINT8(85, batteryPercentFromMv(4000));
  // halfway between 3700 (25) and 3750 (40) -> 32 or 33
  uint8_t mid = batteryPercentFromMv(3725);
  TEST_ASSERT_TRUE(mid == 32 || mid == 33);
}

void test_battery_redraw_rules() {
  TEST_ASSERT_FALSE(batteryRedrawDue(80, 80, 0, 600000));      // no change, never
  TEST_ASSERT_FALSE(batteryRedrawDue(80, 78, 0, 60000));       // small change, too soon
  TEST_ASSERT_TRUE(batteryRedrawDue(80, 75, 0, 60000));        // 5% change
  TEST_ASSERT_TRUE(batteryRedrawDue(80, 79, 0, 300000));       // any change after 5 min
}

void test_button_policy() {
  TEST_ASSERT_EQUAL(ButtonAction::Drop, decideButton(ButtonId::Left, BuzzerMode::Off, false));
  TEST_ASSERT_EQUAL(ButtonAction::Drop, decideButton(ButtonId::Green, BuzzerMode::Dismissable, false));
  TEST_ASSERT_EQUAL(ButtonAction::SendButton, decideButton(ButtonId::Left, BuzzerMode::Off, true));
  TEST_ASSERT_EQUAL(ButtonAction::SendButton, decideButton(ButtonId::Green, BuzzerMode::On, true));
  TEST_ASSERT_EQUAL(ButtonAction::SendButton, decideButton(ButtonId::Right, BuzzerMode::Dismissable, true));
  TEST_ASSERT_EQUAL(ButtonAction::DismissBuzzer, decideButton(ButtonId::Green, BuzzerMode::Dismissable, true));
}

void test_buzzer_pattern() {
  BuzzerPattern p;
  TEST_ASSERT_FALSE(p.running());
  TEST_ASSERT_FALSE(p.toneOn());
  p.start();
  TEST_ASSERT_TRUE(p.running());
  TEST_ASSERT_TRUE(p.toneOn());
  p.toggle();
  TEST_ASSERT_FALSE(p.toneOn());
  p.toggle();
  TEST_ASSERT_TRUE(p.toneOn());
  p.stop();
  TEST_ASSERT_FALSE(p.running());
  TEST_ASSERT_FALSE(p.toneOn());
  p.toggle();
  TEST_ASSERT_FALSE(p.toneOn());
  TEST_ASSERT_EQUAL_UINT32(300, BuzzerPattern::kStepMs);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_battery_curve_edges);
  RUN_TEST(test_battery_curve_points_and_interpolation);
  RUN_TEST(test_battery_redraw_rules);
  RUN_TEST(test_button_policy);
  RUN_TEST(test_buzzer_pattern);
  return UNITY_END();
}
