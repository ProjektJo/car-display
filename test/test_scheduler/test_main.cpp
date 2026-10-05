// PID-Scheduler (A7): Takt-Klassen, nie nicht unterstützte PIDs, CAN bis 6 PIDs je Anfrage,
// KWP einzeln, Sprint nur Tempo/Drehzahl/Gaspedal.
#include <unity.h>

#include <cstring>
#include <initializer_list>
#include <set>
#include <vector>

#include "config.h"
#include "obd/pid_scheduler.h"

void setUp() {}
void tearDown() {}

static void support(uint8_t bits[32], std::initializer_list<uint8_t> pids) {
  memset(bits, 0, 32);
  for (uint8_t p : pids) bits[p / 8] |= static_cast<uint8_t>(1u << (p % 8));
}

// Vermuteter Renault Modus 1.2 (A7): MAP, kein MAF, kein 0x5E, kein 0x2F, kein 0x49
static void modus(uint8_t bits[32]) {
  support(bits, {0x01, 0x03, 0x04, 0x05, 0x06, 0x07, 0x0B, 0x0C, 0x0D, 0x0F, 0x11, 0x20});
}

// Alle Anfragen einer Runde
static std::vector<ObdRequest> round(PidScheduler& s, uint32_t now) {
  std::vector<ObdRequest> out;
  do {
    out.push_back(s.next(now));
  } while (!s.roundFinished() && out.size() < 40);
  return out;
}

static std::set<int> pidsOf(const std::vector<ObdRequest>& reqs, bool& voltage) {
  std::set<int> s;
  voltage = false;
  for (const auto& r : reqs) {
    if (r.voltage) voltage = true;
    for (int i = 0; i < r.count; i++) s.insert(r.pids[i]);
  }
  return s;
}

void test_unsupported_never_polled() {
  uint8_t bits[32];
  modus(bits);
  PidScheduler s;
  s.reset(bits, false, cfg::OBD_MAX_PIDS_PER_REQUEST);
  TEST_ASSERT_FALSE(s.schedules(0x10));  // MAF
  TEST_ASSERT_FALSE(s.schedules(0x5E));
  TEST_ASSERT_FALSE(s.schedules(0x2F));
  TEST_ASSERT_FALSE(s.schedules(0x49));
  TEST_ASSERT_FALSE(s.schedules(0x44));
  PidClass c;
  TEST_ASSERT_TRUE(s.classOf(0x11, c));
  TEST_ASSERT_TRUE(c == PidClass::Fast);  // ohne Gaspedal ist die Drosselklappe schnell
}

void test_kwp_one_pid_per_request_and_classes() {
  uint8_t bits[32];
  modus(bits);
  PidScheduler s;
  s.reset(bits, false, cfg::OBD_MAX_PIDS_PER_REQUEST);

  // Erste Runde: alles einmal (schnell, mittel, langsam mit ATRV, selten)
  bool volt = false;
  auto r1 = round(s, 1000);
  for (const auto& r : r1) TEST_ASSERT_TRUE(r.voltage || r.count == 1);
  auto p1 = pidsOf(r1, volt);
  TEST_ASSERT_TRUE(volt);
  TEST_ASSERT_EQUAL(11, (int)p1.size());  // alle unterstützten außer dem Verweis 0x20

  // 0,5 s später: nur schnelle
  auto p2 = pidsOf(round(s, 1500), volt);
  TEST_ASSERT_FALSE(volt);
  TEST_ASSERT_TRUE((p2 == std::set<int>{0x0D, 0x0C, 0x0B, 0x11}));

  // 1 s nach der ersten Runde: schnelle und mittlere
  auto p3 = pidsOf(round(s, 2000), volt);
  TEST_ASSERT_FALSE(volt);
  TEST_ASSERT_TRUE(p3.count(0x0F) && p3.count(0x06) && p3.count(0x07) && p3.count(0x03) && p3.count(0x04));
  TEST_ASSERT_FALSE(p3.count(0x05) || p3.count(0x01));

  // 5 s: langsame mit ATRV, 30 s: seltene
  auto p4 = pidsOf(round(s, 6000), volt);
  TEST_ASSERT_TRUE(volt);
  TEST_ASSERT_TRUE(p4.count(0x05));
  TEST_ASSERT_FALSE(p4.count(0x01));
  auto p5 = pidsOf(round(s, 31000), volt);
  TEST_ASSERT_TRUE(p5.count(0x01));
}

void test_can_up_to_six_per_request() {
  uint8_t bits[32];
  support(bits, {0x01, 0x03, 0x04, 0x05, 0x06, 0x07, 0x0B, 0x0C, 0x0D, 0x0F, 0x10, 0x11, 0x2F, 0x44, 0x49, 0x5E});
  PidScheduler s;
  s.reset(bits, true, cfg::OBD_MAX_PIDS_PER_REQUEST);
  bool sawSix = false;
  int requests = 0;
  for (const auto& r : round(s, 1000)) {
    requests++;
    TEST_ASSERT_TRUE(r.count <= 6);
    TEST_ASSERT_TRUE(!r.voltage || r.count == 0);  // ATRV immer allein
    sawSix = sawSix || r.count == 6;
  }
  TEST_ASSERT_TRUE(sawSix);
  TEST_ASSERT_TRUE(requests <= 5);  // 16 Einträge in höchstens 5 Anfragen
  PidClass c;
  TEST_ASSERT_TRUE(s.classOf(0x49, c) && c == PidClass::Fast);
  TEST_ASSERT_TRUE(s.classOf(0x11, c) && c == PidClass::Medium);  // mit Gaspedal reicht mittel
  TEST_ASSERT_TRUE(s.classOf(0x5E, c) && c == PidClass::Fast);
}

void test_sprint_only_speed_rpm_pedal() {
  uint8_t bits[32];
  support(bits, {0x01, 0x05, 0x0B, 0x0C, 0x0D, 0x0F, 0x11, 0x49});
  PidScheduler s;
  s.reset(bits, true, cfg::OBD_MAX_PIDS_PER_REQUEST);
  s.next(0);
  s.setSprint(true);
  for (uint32_t t = 100; t < 40000; t += 100) {
    const ObdRequest r = s.next(t);
    TEST_ASSERT_FALSE(r.voltage);
    for (int i = 0; i < r.count; i++)
      TEST_ASSERT_TRUE(r.pids[i] == 0x0D || r.pids[i] == 0x0C || r.pids[i] == 0x49);
  }
  s.setSprint(false);
  bool volt = false;
  auto p = pidsOf(round(s, 41000), volt);
  TEST_ASSERT_TRUE(p.count(0x05) && p.count(0x01) && volt);  // danach sofort wieder alles Fällige
}

void test_nothing_supported_only_voltage() {
  uint8_t bits[32] = {};
  PidScheduler s;
  s.reset(bits, false, cfg::OBD_MAX_PIDS_PER_REQUEST);
  for (int i = 0; i < 5; i++) TEST_ASSERT_TRUE(s.next(i * 1000).voltage);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_unsupported_never_polled);
  RUN_TEST(test_kwp_one_pid_per_request_and_classes);
  RUN_TEST(test_can_up_to_six_per_request);
  RUN_TEST(test_sprint_only_speed_rpm_pedal);
  RUN_TEST(test_nothing_supported_only_voltage);
  return UNITY_END();
}

#include "../board_runner.h"
