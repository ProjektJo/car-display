// BOOT-Taste: kurz = nächste Seite, lang (0,8 s) = Menü, im Simulator sehr lang (2 s) = Vollgas-Sequenz
#include <unity.h>

#include "config.h"
#include "util/button_logic.h"

void setUp() {}
void tearDown() {}

// Taste ab t0 für heldMs drücken, in 10-ms-Schritten abtasten; liefert alle Ereignisse in Reihenfolge
static int press(ButtonLogic& b, uint32_t t0, uint32_t heldMs, ButtonEvent* events, int maxEvents) {
  int n = 0;
  for (uint32_t t = t0; t < t0 + heldMs + 200; t += 10) {
    const ButtonEvent e = b.update(t < t0 + heldMs, t);
    if (e != ButtonEvent::None && n < maxEvents) events[n++] = e;
  }
  return n;
}

void test_short_press() {
  ButtonLogic b(cfg::BUTTON_DEBOUNCE_MS, cfg::BUTTON_LONG_MS, 0);
  ButtonEvent ev[4];
  TEST_ASSERT_EQUAL(1, press(b, 1000, 200, ev, 4));
  TEST_ASSERT_TRUE(ev[0] == ButtonEvent::Short);
}

void test_long_press_fires_while_held() {
  ButtonLogic b(cfg::BUTTON_DEBOUNCE_MS, cfg::BUTTON_LONG_MS, 0);
  ButtonEvent ev[4];
  TEST_ASSERT_EQUAL(1, press(b, 1000, 1500, ev, 4));
  TEST_ASSERT_TRUE(ev[0] == ButtonEvent::Long);
  // genau beim Erreichen von 0,8 s (+ Entprellzeit), noch während gedrückt
  ButtonLogic b2(cfg::BUTTON_DEBOUNCE_MS, cfg::BUTTON_LONG_MS, 0);
  uint32_t firedAt = 0;
  for (uint32_t t = 0; t < 1500; t += 10)
    if (b2.update(true, t) == ButtonEvent::Long) firedAt = t;
  TEST_ASSERT_UINT32_WITHIN(20, cfg::BUTTON_DEBOUNCE_MS + cfg::BUTTON_LONG_MS, firedAt);
}

void test_bounce_is_ignored() {
  ButtonLogic b(cfg::BUTTON_DEBOUNCE_MS, cfg::BUTTON_LONG_MS, 0);
  int events = 0;
  // 3 kurze Prellimpulse von 10 ms
  for (uint32_t t = 0; t < 200; t += 5) {
    const bool level = (t >= 10 && t < 20) || (t >= 40 && t < 50) || (t >= 70 && t < 80);
    if (b.update(level, t) != ButtonEvent::None) events++;
  }
  TEST_ASSERT_EQUAL(0, events);
}

void test_simulator_long_and_very_long() {
  ButtonLogic b(cfg::BUTTON_DEBOUNCE_MS, cfg::BUTTON_LONG_MS, cfg::BUTTON_SIM_SPRINT_MS);
  ButtonEvent ev[4];
  TEST_ASSERT_EQUAL(1, press(b, 1000, 1200, ev, 4));
  TEST_ASSERT_TRUE(ev[0] == ButtonEvent::Long);  // erst beim Loslassen
  TEST_ASSERT_EQUAL(1, press(b, 5000, 2600, ev, 4));
  TEST_ASSERT_TRUE(ev[0] == ButtonEvent::VeryLong);
  TEST_ASSERT_EQUAL(1, press(b, 9000, 150, ev, 4));
  TEST_ASSERT_TRUE(ev[0] == ButtonEvent::Short);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_short_press);
  RUN_TEST(test_long_press_fires_while_held);
  RUN_TEST(test_bounce_is_ignored);
  RUN_TEST(test_simulator_long_and_very_long);
  return UNITY_END();
}

#include "../board_runner.h"
