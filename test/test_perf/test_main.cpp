// Sprintmessung und Auto-Sprint (A10): Prüfwerte aus dem Master-Prompt
#include "calc/perf.h"

#include <cmath>
#include <unity.h>

void setUp() {}
void tearDown() {}

using perf::State;

// Fahrt mit 8 Messungen je Sekunde: Stand, dann gleichmäßige Beschleunigung accel km/h je s.
// Pedal relativ (0 leer … 100 Vollgas), die Beschleunigung geht in m/s² mit.
struct Drive {
  perf::SprintMeter m;
  uint32_t t = 1000;
  float v = 0;
  void step(float accelKmhS, float pedal) {
    t += 125;
    v += accelKmhS * 0.125f;
    if (v < 0) v = 0;
    m.update(t, v, pedal, accelKmhS / 3.6f);
  }
  void stand(float seconds, float pedal = 0) {
    for (int i = 0; i < static_cast<int>(seconds * 8); i++) step(0, pedal);
  }
};

// Pedal 98 %, 3800 U/min nach 1,2 s Stand -> gültig, Zielzeit interpoliert
void test_sprint_valid() {
  Drive d;
  d.stand(1.2f);
  d.stand(0.5f, 98);  // Vollgas im Stand: Sprint erkannt
  TEST_ASSERT_EQUAL_UINT16(1, d.m.launchSeq());
  while (d.v < 101) d.step(8.0f, 98);  // 0–100 in 12,5 s
  TEST_ASSERT_EQUAL_INT((int)State::Done, (int)d.m.state());
  TEST_ASSERT_FLOAT_WITHIN(0.2f, 12.5f, d.m.last100());
  TEST_ASSERT_FLOAT_WITHIN(0.2f, 6.25f, d.m.last50());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, d.m.last100(), d.m.best().s100);
  TEST_ASSERT_TRUE(d.m.takeBestChanged());
  TEST_ASSERT_EQUAL_UINT8(perf::TRACE_POINTS, d.m.lastTrace().count);
  TEST_ASSERT_EQUAL_UINT16(2, d.m.resultSeq());  // 0–50 und 0–100
  TEST_ASSERT_EQUAL_INT((int)perf::Kind::S100, (int)d.m.resultKind());
  TEST_ASSERT_FLOAT_IS_NAN(d.m.resultPrevBest());
}

// Kräftig losgefahren, dann zu schwach (1 km/h je s = 0,28 m/s²): Abbruch nach 3 s
void test_sprint_weak_abort() {
  Drive d;
  d.stand(1.5f);
  while (d.v < 40) d.step(10.0f, 98);
  TEST_ASSERT_EQUAL_INT((int)State::Running, (int)d.m.state());
  for (int i = 0; i < 8 * 4; i++) d.step(1.0f, 98);
  TEST_ASSERT_EQUAL_INT((int)State::Ready, (int)d.m.state());
}

// Ohne Pedal: 2 m/s² (7,5 km/h je s) zählt als Sprint
void test_sprint_2ms2() {
  Drive d;
  d.stand(1.5f, NAN);
  while (d.v < 30) d.step(7.5f, NAN);
  TEST_ASSERT_EQUAL_INT((int)State::Running, (int)d.m.state());
}

// Ohne Pedalwert: kräftige Beschleunigung (12 km/h je s) zählt als Sprint
void test_sprint_by_accel() {
  Drive d;
  d.stand(1.5f, NAN);
  while (d.v < 101) d.step(12.0f, NAN);
  TEST_ASSERT_EQUAL_UINT16(1, d.m.launchSeq());
  TEST_ASSERT_EQUAL_INT((int)State::Done, (int)d.m.state());
  TEST_ASSERT_FLOAT_WITHIN(0.2f, 8.33f, d.m.last100());
}

// Normales Anfahren: Live-Kurve läuft (Waiting), zählt aber nicht
void test_sprint_live_curve() {
  Drive d;
  d.stand(2.0f);
  while (d.v < 30) d.step(5.0f, 45);
  TEST_ASSERT_EQUAL_INT((int)State::Waiting, (int)d.m.state());
  TEST_ASSERT_TRUE(d.m.lastTrace().count >= 6);
  TEST_ASSERT_TRUE(d.m.active());
}

// Normales Anfahren mit 45 % wird still verworfen
void test_sprint_normal_start_discarded() {
  Drive d;
  d.stand(2.0f);
  while (d.v < 100) d.step(5.0f, 45);
  TEST_ASSERT_EQUAL_UINT16(0, d.m.launchSeq());
  TEST_ASSERT_EQUAL_INT((int)State::Ready, (int)d.m.state());
  TEST_ASSERT_FLOAT_IS_NAN(d.m.last100());
  TEST_ASSERT_FLOAT_IS_NAN(d.m.best().s100);
}

// Schaltpause ohne Gas bricht nicht ab; länger als 30 s bis 100 bricht ab
void test_sprint_shift_pause() {
  Drive d;
  d.stand(1.5f);
  while (d.v < 30) d.step(10.0f, 98);
  for (int i = 0; i < 5; i++) d.step(0, 0);  // 0,625 s Schaltpause
  TEST_ASSERT_EQUAL_INT((int)State::Running, (int)d.m.state());
  while (d.v < 101) d.step(10.0f, 98);
  TEST_ASSERT_EQUAL_INT((int)State::Done, (int)d.m.state());

  Drive e;
  e.stand(1.5f);
  while (e.v < 30) e.step(10.0f, 98);
  for (int i = 0; i < 8 * 31; i++) e.step(0, 20);  // 31 s bei 30 km/h
  TEST_ASSERT_EQUAL_INT((int)State::Ready, (int)e.m.state());
}

// Tempo fällt um mehr als 3 km/h: Abbruch
void test_sprint_speed_drop() {
  Drive d;
  d.stand(1.5f);
  while (d.v < 40) d.step(10.0f, 98);
  for (int i = 0; i < 8; i++) d.step(-4.0f, 98);  // −4 km/h
  TEST_ASSERT_EQUAL_INT((int)State::Ready, (int)d.m.state());
}

// 80–120: startet beim Durchfahren von 80 mit Gas ≥ 80 % oder ab 1,5 m/s²
void test_sprint_80_120() {
  Drive d;
  d.stand(1.0f);
  while (d.v < 70) d.step(5.0f, 40);
  while (d.v < 121) d.step(4.0f, 95);  // 80–120 in 10 s
  TEST_ASSERT_FLOAT_WITHIN(0.2f, 10.0f, d.m.last80120());
  TEST_ASSERT_FALSE(d.m.run80());

  Drive e;  // ohne Pedal, 4 km/h je s = 1,1 m/s²: keine Messung
  e.stand(1.0f);
  while (e.v < 70) e.step(5.0f, NAN);
  while (e.v < 121) e.step(4.0f, NAN);
  TEST_ASSERT_FLOAT_IS_NAN(e.m.last80120());

  Drive f;  // ohne Pedal, 6 km/h je s = 1,7 m/s²: Messung
  f.stand(1.0f);
  while (f.v < 70) f.step(5.0f, NAN);
  while (f.v < 121) f.step(6.0f, NAN);
  TEST_ASSERT_FLOAT_WITHIN(0.2f, 6.67f, f.m.last80120());
}

// Auto-Sprint: Ziel erreicht -> Rücksprung nach 4 s; manueller Seitenwechsel hebt ihn auf
void test_auto_sprint_return() {
  perf::AutoSprint a;
  uint32_t t = 1000;
  a.update(t, true, 0, State::Ready, 0, 0, false);  // Startzustand merken
  TEST_ASSERT_EQUAL_INT((int)perf::AutoSprint::Action::ShowSprint, (int)a.update(t += 100, true, 1, State::Running, 0, 98, false));
  for (; t < 12000; t += 100) TEST_ASSERT_EQUAL_INT((int)perf::AutoSprint::Action::None, (int)a.update(t, true, 1, State::Running, 0, 98, false));
  const uint32_t done = t;
  for (; t < done + 3900; t += 100) TEST_ASSERT_EQUAL_INT((int)perf::AutoSprint::Action::None, (int)a.update(t, true, 1, State::Done, done, 98, false));
  TEST_ASSERT_EQUAL_INT((int)perf::AutoSprint::Action::Return, (int)a.update(done + 4000, true, 1, State::Done, done, 98, false));

  // Fahrer wischt selbst: kein Rücksprung
  perf::AutoSprint b;
  t = 1000;
  b.update(t, true, 0, State::Ready, 0, 0, false);
  TEST_ASSERT_EQUAL_INT((int)perf::AutoSprint::Action::ShowSprint, (int)b.update(t += 100, true, 1, State::Running, 0, 98, false));
  b.cancel();
  for (; t < 40000; t += 100) TEST_ASSERT_EQUAL_INT((int)perf::AutoSprint::Action::None, (int)b.update(t, true, 1, State::Done, 5000, 0, false));

  // Ausgeschaltet bzw. Fenster offen: kein Wechsel
  perf::AutoSprint c;
  c.update(1000, false, 0, State::Ready, 0, 0, false);
  TEST_ASSERT_EQUAL_INT((int)perf::AutoSprint::Action::None, (int)c.update(1100, false, 1, State::Running, 0, 98, false));
  TEST_ASSERT_EQUAL_INT((int)perf::AutoSprint::Action::None, (int)c.update(1200, true, 2, State::Running, 0, 98, true));
}

// Gas fällt 2 s unter 50 %: zurück; spätestens nach 25 s
void test_auto_sprint_low_and_max() {
  perf::AutoSprint a;
  uint32_t t = 1000;
  a.update(t, true, 0, State::Ready, 0, 0, false);
  a.update(t += 100, true, 1, State::Running, 0, 98, false);
  for (; t < 3000; t += 100) a.update(t, true, 1, State::Running, 0, 98, false);
  perf::AutoSprint::Action r = perf::AutoSprint::Action::None;
  const uint32_t lift = t;
  // Rohpedal unter 50 %, die Messung läuft aber noch: kein Rücksprung (manche Autos melden Vollgas unter 50 %)
  for (; t < lift + 3000 && r == perf::AutoSprint::Action::None; t += 100) r = a.update(t, true, 1, State::Running, 0, 20, false);
  TEST_ASSERT_EQUAL_INT((int)perf::AutoSprint::Action::None, (int)r);
  // Messung vorbei: zurück
  r = a.update(t, true, 1, State::Ready, 0, 20, false);
  TEST_ASSERT_EQUAL_INT((int)perf::AutoSprint::Action::Return, (int)r);

  perf::AutoSprint b;
  t = 1000;
  b.update(t, true, 0, State::Ready, 0, 0, false);
  b.update(t += 100, true, 1, State::Running, 0, 98, false);
  r = perf::AutoSprint::Action::None;
  for (; t < 30000 && r == perf::AutoSprint::Action::None; t += 100) r = b.update(t, true, 1, State::Running, 0, 98, false);
  TEST_ASSERT_UINT32_WITHIN(150, 1100 + 25000, t - 100);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_sprint_valid);
  RUN_TEST(test_sprint_normal_start_discarded);
  RUN_TEST(test_sprint_by_accel);
  RUN_TEST(test_sprint_weak_abort);
  RUN_TEST(test_sprint_2ms2);
  RUN_TEST(test_sprint_live_curve);
  RUN_TEST(test_sprint_shift_pause);
  RUN_TEST(test_sprint_speed_drop);
  RUN_TEST(test_sprint_80_120);
  RUN_TEST(test_auto_sprint_return);
  RUN_TEST(test_auto_sprint_low_and_max);
  return UNITY_END();
}

#include "../board_runner.h"
