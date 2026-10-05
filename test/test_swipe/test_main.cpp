// Wischgesten mit den Schwellen der Vorschau: 40 px waagerecht, 50 px nach unten aus den obersten 40 px
#include <unity.h>

#include "config.h"
#include "util/swipe.h"

void setUp() {}
void tearDown() {}

static SwipeDetector make() {
  return SwipeDetector(cfg::TAP_SLOP_PX, cfg::SWIPE_MIN_DX_PX, cfg::SWIPE_DOWN_MIN_DY_PX, cfg::SWIPE_DOWN_START_MAX_Y_PX);
}

// Finger in 8 Schritten von (x0,y0) nach (x1,y1), dann loslassen
static Swipe drag(SwipeDetector& d, int x0, int y0, int x1, int y1) {
  for (int i = 0; i <= 8; i++) d.update(true, x0 + (x1 - x0) * i / 8, y0 + (y1 - y0) * i / 8);
  return d.update(false, x1, y1);
}

void test_left_and_right() {
  SwipeDetector d = make();
  TEST_ASSERT_TRUE(drag(d, 250, 120, 150, 130) == Swipe::Left);   // Finger nach links = nächste Seite
  TEST_ASSERT_TRUE(drag(d, 100, 120, 200, 110) == Swipe::Right);
}

void test_too_short_or_too_steep() {
  SwipeDetector d = make();
  TEST_ASSERT_TRUE(drag(d, 200, 120, 165, 120) == Swipe::None);   // nur 35 px
  TEST_ASSERT_TRUE(drag(d, 200, 60, 150, 180) == Swipe::None);    // mehr senkrecht als waagerecht, nicht von oben
}

void test_down_from_top_only() {
  SwipeDetector d = make();
  TEST_ASSERT_TRUE(drag(d, 160, 10, 165, 90) == Swipe::Down);
  TEST_ASSERT_TRUE(drag(d, 160, 60, 165, 160) == Swipe::None);    // beginnt zu tief
}

void test_tap_is_not_moved() {
  SwipeDetector d = make();
  d.update(true, 100, 100);
  d.update(true, 105, 104);
  TEST_ASSERT_FALSE(d.movedWhileDown());
  d.update(true, 120, 100);
  TEST_ASSERT_TRUE(d.movedWhileDown());
  TEST_ASSERT_TRUE(d.update(false, 120, 100) == Swipe::None);
  TEST_ASSERT_FALSE(d.movedWhileDown());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_left_and_right);
  RUN_TEST(test_too_short_or_too_steep);
  RUN_TEST(test_down_from_top_only);
  RUN_TEST(test_tap_is_not_moved);
  return UNITY_END();
}

#include "../board_runner.h"
