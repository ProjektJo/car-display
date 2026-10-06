// Optionale Sensoren (Etappe 8): Ortszeit, Sonnenstand, Einbaulage und Steigung des MPU6050
#include "sensors/imu_math.h"
#include "sensors/sun_time.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

using suntime::DateTime;

void test_calendar() {
  TEST_ASSERT_EQUAL_INT(0, (int)suntime::daysFromCivil(1970, 1, 1));
  TEST_ASSERT_EQUAL_INT(2, suntime::weekday(2026, 10, 6));   // Dienstag
  TEST_ASSERT_EQUAL_INT(0, suntime::weekday(2026, 10, 25));  // Sonntag
  const DateTime t{2026, 10, 6, 21, 5, 9};
  const DateTime u = suntime::fromEpoch(suntime::toEpoch(t));
  TEST_ASSERT_EQUAL_INT(2026, u.year);
  TEST_ASSERT_EQUAL_INT(10, u.month);
  TEST_ASSERT_EQUAL_INT(6, u.day);
  TEST_ASSERT_EQUAL_INT(21, u.hour);
  TEST_ASSERT_EQUAL_INT(9, u.second);
}

// Sommerzeit: letzter Sonntag im März 01:00 UTC bis letzter Sonntag im Oktober 01:00 UTC
void test_local_time() {
  DateTime l = suntime::toLocal(DateTime{2026, 7, 1, 12, 0, 0});
  TEST_ASSERT_EQUAL_INT(14, l.hour);
  l = suntime::toLocal(DateTime{2026, 12, 31, 23, 30, 0});  // Silvester: schon 0:30 am 1.1.
  TEST_ASSERT_EQUAL_INT(2027, l.year);
  TEST_ASSERT_EQUAL_INT(0, l.hour);
  l = suntime::toLocal(DateTime{2026, 10, 25, 0, 59, 0});   // noch Sommerzeit
  TEST_ASSERT_EQUAL_INT(2, l.hour);
  l = suntime::toLocal(DateTime{2026, 10, 25, 1, 0, 0});    // Winterzeit
  TEST_ASSERT_EQUAL_INT(2, l.hour);
  l = suntime::toLocal(DateTime{2026, 3, 29, 1, 0, 0});     // Beginn der Sommerzeit
  TEST_ASSERT_EQUAL_INT(3, l.hour);
}

// Berlin: 21.6. Aufgang ca. 02:43 UTC, Untergang ca. 19:33 UTC; 21.12. ca. 07:15 und 14:54 UTC
void test_sun_times() {
  float rise = 0, set = 0;
  int polar = 9;
  suntime::sunTimes(2026, 6, 21, 52.52f, 13.405f, rise, set, polar);
  TEST_ASSERT_EQUAL_INT(0, polar);
  TEST_ASSERT_FLOAT_WITHIN(6, 2 * 60 + 43, rise);
  TEST_ASSERT_FLOAT_WITHIN(6, 19 * 60 + 33, set);
  suntime::sunTimes(2026, 12, 21, 52.52f, 13.405f, rise, set, polar);
  TEST_ASSERT_FLOAT_WITHIN(6, 7 * 60 + 15, rise);
  TEST_ASSERT_FLOAT_WITHIN(6, 14 * 60 + 54, set);
  // Polarnacht
  suntime::sunTimes(2026, 12, 21, 78.0f, 15.0f, rise, set, polar);
  TEST_ASSERT_EQUAL_INT(-1, polar);
}

// Tag/Nacht mit 15 min Übergang
void test_night_factor() {
  TEST_ASSERT_EQUAL_FLOAT(0.0f, suntime::nightFactor(DateTime{2026, 6, 21, 12, 0, 0}, 52.52f, 13.405f));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, suntime::nightFactor(DateTime{2026, 6, 21, 23, 0, 0}, 52.52f, 13.405f));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, suntime::nightFactor(DateTime{2026, 6, 21, 1, 0, 0}, 52.52f, 13.405f));
  float rise = 0, set = 0;
  int polar = 0;
  suntime::sunTimes(2026, 6, 21, 52.52f, 13.405f, rise, set, polar);
  const int m = static_cast<int>(set);
  const float f = suntime::nightFactor(DateTime{2026, 6, 21, m / 60, m % 60, 0}, 52.52f, 13.405f);
  TEST_ASSERT_TRUE(f > 0.3f && f < 0.7f);  // genau beim Untergang mitten im Übergang
}

// Einbaulage: Sensor liegt auf der Seite (oben = -y), vorne = +z; dann Steigung aus OBD-Vergleich
void test_imu_learn_and_slope() {
  const float g = 9.81f;
  imu::Learner l;
  for (int i = 0; i < 400; i++) l.add({0, -g, 0}, {0, 0, 0}, 0, 0, 0.01f);  // 4 s Stillstand
  TEST_ASSERT_TRUE(l.hasUp());
  TEST_ASSERT_FALSE(l.done());
  for (int i = 0; i < 200; i++) l.add({0, -g, 2.0f}, {0, 0, 0}, 20, 2.0f, 0.01f);  // geradeaus 2 m/s²
  TEST_ASSERT_TRUE(l.done());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.0f, l.axes().up.y);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, l.axes().fwd.z);

  imu::Fusion f;
  f.setAxes(l.axes());
  // 5 % bergauf, konstante Fahrt: Sensor misst längs g·sin(Steigung), OBD 0
  const float s = std::atan(0.05f);
  const imu::Vec a{0, -g * std::cos(s), g * std::sin(s)};
  for (int i = 0; i < 3000; i++) f.add(a, {0, 0, 0}, 50, 0.0f, 0.01f);  // 30 s
  TEST_ASSERT_FLOAT_WITHIN(0.3f, 5.0f, f.slopePct());
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, f.longMs2());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, f.latMs2());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_calendar);
  RUN_TEST(test_local_time);
  RUN_TEST(test_sun_times);
  RUN_TEST(test_night_factor);
  RUN_TEST(test_imu_learn_and_slope);
  return UNITY_END();
}

#include "../board_runner.h"
