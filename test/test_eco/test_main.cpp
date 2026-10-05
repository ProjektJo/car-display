// Eco-Logik (A6, A9, A10): Prüfwerte aus dem Master-Prompt (Tabelle Unit-Tests)
#include "calc/eco.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

// Rollen 20 %, Pedal 2 %/s, Schaltempfehlung 5 % offen, gebremst 0,75 l/100 km, Leerlauf 10 %
// -> Teile 80 / 75 / 80 / 50 / 66,7 -> Score 74
void test_score() {
  eco::ScoreInput in;
  in.moveS = 1000;
  in.rollS = 200;
  in.pedalAbs = 2000;
  in.shiftOpenS = 50;
  in.km = 20;
  in.brakedL = 0.15f;  // 0,75 l/100 km
  in.driveS = 1000;
  in.idleS = 100;
  const eco::ScoreParts p = eco::score(in);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 80.0f, p.roll);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 75.0f, p.calm);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 80.0f, p.early);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, p.brake);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 66.7f, p.idle);
  TEST_ASSERT_EQUAL_FLOAT(74.0f, p.score);
  // Grenzen: jeder Teil 0–100, ohne Fahrzeit kein Score
  in.rollS = 900;
  in.pedalAbs = 20000;
  TEST_ASSERT_EQUAL_FLOAT(100.0f, eco::score(in).roll);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, eco::score(in).calm);
  eco::ScoreInput none;
  TEST_ASSERT_FLOAT_IS_NAN(eco::score(none).score);
}

// 1150 kg, 50 -> 30 km/h in 4 s, cw·A 0,70: Bremsleistung > 0, Liter = J ÷ 8 MJ
void test_brake_energy() {
  const float a = (30.0f - 50.0f) / 3.6f / 4.0f;  // -1,39 m/s²
  const float p = eco::brakePowerW(1150, 0.70f, 40.0f / 3.6f, a);
  TEST_ASSERT_TRUE(p > 0);
  // von Hand: (1150 · 1,389 − ½ · 1,2 · 0,7 · 11,1² − 0,012 · 1150 · 9,81) · 11,1
  const float v = 40.0f / 3.6f;
  const float expect = (1150 * -a - 0.5f * 1.2f * 0.7f * v * v - 0.012f * 1150 * 9.81f) * v;
  TEST_ASSERT_FLOAT_WITHIN(1.0f, expect, p);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, eco::litersFromJoule(8e6, 32.0f));
  // natürliches Ausrollen ist kein Bremsen
  TEST_ASSERT_EQUAL_FLOAT(0.0f, eco::brakePowerW(1150, 0.70f, 80.0f / 3.6f, -0.25f));  // Ausrollen bei 80: ≈ 0,3 m/s²
}

// 50 km/h, 2 m/s², 1150 kg, cw·A 0,70 -> ≈ 35 kW
void test_power() {
  const float p = eco::wheelPowerW(1150, 0.70f, 50.0f / 3.6f, 2.0f);
  TEST_ASSERT_FLOAT_WITHIN(500.0f, 35000.0f, p);
}

// 2300 U/min, Gang 2 (k 13), Gang 3 (k 19,5), Pedal 40 % -> ja (nächster Gang 1533 U/min); 1800 -> nein
void test_shift_advice() {
  const float gears[] = {7.3f, 13.0f, 19.5f, 26.0f, 32.0f};
  TEST_ASSERT_TRUE(eco::shiftAdvice(2300, 40, false, false, gears, 5, 1, 2200, 1300));
  TEST_ASSERT_FALSE(eco::shiftAdvice(1800, 40, false, false, gears, 5, 1, 2200, 1300));
  // nicht im höchsten Gang, nicht bei viel Gas, nicht im Schub, nicht ohne Gas
  TEST_ASSERT_FALSE(eco::shiftAdvice(2300, 40, false, false, gears, 5, 4, 2200, 1300));
  TEST_ASSERT_FALSE(eco::shiftAdvice(2300, 65, false, false, gears, 5, 1, 2200, 1300));
  TEST_ASSERT_FALSE(eco::shiftAdvice(2300, 40, false, true, gears, 5, 1, 2200, 1300));
  TEST_ASSERT_FALSE(eco::shiftAdvice(2300, 40, true, false, gears, 5, 1, 2200, 1300));
  // nächster Gang käme unter 1300 U/min: 2200 · 7,3 / 13 = 1235
  TEST_ASSERT_FALSE(eco::shiftAdvice(2200, 40, false, false, gears, 5, 0, 2200, 1300));
  // ohne gelernte Gänge nur nach Drehzahl; mit Gängen, aber Kupplung getreten, nie
  TEST_ASSERT_TRUE(eco::shiftAdvice(2300, 40, false, false, gears, 0, -1, 2200, 1300));
  TEST_ASSERT_FALSE(eco::shiftAdvice(2300, 40, false, false, gears, 5, -1, 2200, 1300));
}

// Histogramm mit Häufungen bei 7,3 / 13,2 / 19,5 / 26 / 32 -> 5 Gänge, Zuordnung ± 6 %
void test_gear_learning() {
  static eco::GearHistogram h;
  h.clear();
  const float centers[] = {7.3f, 13.2f, 19.5f, 26.0f, 32.0f};
  const int counts[] = {150, 400, 600, 800, 1500};
  uint32_t seed = 7;
  for (int g = 0; g < 5; g++)
    for (int i = 0; i < counts[g]; i++) {
      seed = seed * 1103515245u + 12345u;
      const float noise = ((seed >> 8) % 2001) / 1000.0f - 1.0f;  // -1 … 1
      h.add(centers[g] * (1.0f + 0.01f * noise));                // ± 1 %
    }
  // Lücken zwischen den Gängen (Schalten) dürfen nichts ausmachen
  for (int i = 0; i < 30; i++) h.add(10.0f + i * 0.4f);
  float found[8];
  const int n = eco::findGears(h, found, 8);
  TEST_ASSERT_EQUAL_INT(5, n);
  for (int g = 0; g < 5; g++) TEST_ASSERT_FLOAT_WITHIN(centers[g] * 0.02f, centers[g], found[g]);
  // Zuordnung ± 6 %
  TEST_ASSERT_EQUAL_INT(2, eco::matchGear(found, n, 19.5f * 1.05f));
  TEST_ASSERT_EQUAL_INT(-1, eco::matchGear(found, n, 19.5f * 1.10f));
  TEST_ASSERT_EQUAL_INT(3, eco::gearNumber(found, n, 2));
  // ohne 1. Gang (kleinster Wert > 10,5) beginnt die Zählung bei 2
  const float noFirst[] = {13.2f, 19.5f, 26.0f, 32.0f};
  TEST_ASSERT_EQUAL_INT(2, eco::gearNumber(noFirst, 4, 0));
}

void test_display_gear() {
  const float gears[] = {7.3f, 13.2f, 19.5f, 26.0f, 32.0f};
  int idx = -1;
  TEST_ASSERT_EQUAL_INT(3, eco::displayGear(gears, 5, 50.0f, 50.0f / 19.5f * 1000.0f, idx));
  TEST_ASSERT_EQUAL_INT(2, idx);
  // Leerlaufdrehzahl bei 40 km/h: ausgekuppelt = N
  TEST_ASSERT_EQUAL_INT(eco::GEAR_NEUTRAL, eco::displayGear(gears, 5, 40.0f, 800.0f, idx));
  // Kupplung schleift: kein Gang
  TEST_ASSERT_EQUAL_INT(eco::GEAR_NONE, eco::displayGear(gears, 5, 40.0f, 2500.0f, idx));
  // Stand und Motor aus
  TEST_ASSERT_EQUAL_INT(eco::GEAR_NEUTRAL, eco::displayGear(gears, 5, 0.0f, 800.0f, idx));
  TEST_ASSERT_EQUAL_INT(eco::GEAR_NONE, eco::displayGear(gears, 5, 0.0f, 0.0f, idx));
}

void test_stable_k() {
  eco::StableK st;
  uint32_t t = 1000;
  bool stable = false;
  for (int i = 0; i < 20; i++, t += 100) stable = st.add(19.5f + (i % 2) * 0.2f, t);  // ± 1 %
  TEST_ASSERT_TRUE(stable);
  TEST_ASSERT_FALSE(st.add(23.0f, t));  // Sprung > 3 %
  st.reset();
  TEST_ASSERT_FALSE(st.add(19.5f, t + 100));
}

// Fahrzeug-Prüfung: nach ≥ 2 min stabiler Phasen höchstens 1/3 passend -> fragen
void test_gear_check() {
  eco::GearCheck c;
  for (int i = 0; i < 1190; i++) c.add(i % 4 == 0, 0.1f);  // 25 % passend, 119 s
  TEST_ASSERT_FALSE(c.mismatch());
  for (int i = 0; i < 20; i++) c.add(false, 0.1f);
  TEST_ASSERT_TRUE(c.mismatch());
  c.reset();
  for (int i = 0; i < 1500; i++) c.add(i % 2 == 0, 0.1f);  // 50 % passend
  TEST_ASSERT_FALSE(c.mismatch());
}

static eco::TipInput driving(uint32_t now) {
  eco::TipInput in;
  in.nowMs = now;
  in.engineOn = true;
  in.speedKmh = 60;
  in.accelMs2 = 0;
  in.pedalPct = 20;
  in.gear = 5;
  in.coolantC = 90;
  in.rpm = 1800;
  return in;
}

// Gang rein: ausgekuppelt, > 20 km/h, Verzögerung > 0,5 m/s² seit 2 s; weg, sobald ein Gang drin ist
void test_tip_coast() {
  eco::TipEngine e;
  uint32_t t = 1000;
  eco::TipInput in = driving(t);
  in.gear = eco::GEAR_NEUTRAL;
  in.rpm = 800;
  in.accelMs2 = -0.8f;
  in.pedalPct = 0;
  for (; t < 2900; t += 100) {
    in.nowMs = t;
    TEST_ASSERT_EQUAL_INT((int)eco::Tip::None, (int)e.update(in));
  }
  in.nowMs = t + 200;
  TEST_ASSERT_EQUAL_INT((int)eco::Tip::Coast, (int)e.update(in));
  in.gear = 4;  // eingekuppelt: Anlass vorbei
  in.nowMs += 100;
  TEST_ASSERT_EQUAL_INT((int)eco::Tip::None, (int)e.update(in));
  // Segeln, das das Tempo hält, löst nichts aus
  eco::TipEngine e2;
  in = driving(1000);
  in.gear = eco::GEAR_NEUTRAL;
  in.accelMs2 = -0.2f;
  for (uint32_t s = 1000; s < 10000; s += 100) {
    in.nowMs = s;
    TEST_ASSERT_EQUAL_INT((int)eco::Tip::None, (int)e2.update(in));
  }
}

// Langer Stand: nach 60 s, 8 s sichtbar, alle 30 s wieder, weg beim Losfahren; nie im Schub
void test_tip_idle() {
  eco::TipEngine e;
  eco::TipInput in = driving(1000);
  in.speedKmh = 0;
  in.gear = eco::GEAR_NEUTRAL;
  in.rpm = 780;
  uint32_t t = 1000;
  for (; t < 61000; t += 100) {
    in.nowMs = t;
    TEST_ASSERT_EQUAL_INT((int)eco::Tip::None, (int)e.update(in));
  }
  in.nowMs = t + 100;
  TEST_ASSERT_EQUAL_INT((int)eco::Tip::Idle, (int)e.update(in));
  TEST_ASSERT_UINT32_WITHIN(200, 60000, e.standMs(in.nowMs));
  in.nowMs = t + 8200;  // nach 8 s weg
  TEST_ASSERT_EQUAL_INT((int)eco::Tip::None, (int)e.update(in));
  in.nowMs = t + 30200;  // nach 30 s wieder
  TEST_ASSERT_EQUAL_INT((int)eco::Tip::Idle, (int)e.update(in));
  in.speedKmh = 10;  // losgefahren
  in.nowMs += 100;
  TEST_ASSERT_EQUAL_INT((int)eco::Tip::None, (int)e.update(in));
}

// Kalter Motor: solange Kühlmittel < 60 °C und Drehzahl > Kalt-Grenze, auch mit abgeschalteten Tipps
void test_tip_cold() {
  eco::TipEngine e;
  eco::TipInput in = driving(1000);
  in.coolantC = 45;
  in.rpm = 2700;
  in.tipsEnabled = false;
  TEST_ASSERT_EQUAL_INT((int)eco::Tip::Cold, (int)e.update(in));
  in.rpm = 2000;
  in.nowMs += 100;
  TEST_ASSERT_EQUAL_INT((int)eco::Tip::None, (int)e.update(in));
}

// Sperrzeiten: nach einem Tipp 60 s kein anderer, derselbe frühestens nach 5 min; nicht in Ruhezeiten
void test_tip_gaps() {
  eco::TipEngine e;
  eco::TipInput in = driving(1000);
  in.speedKmh = 50;
  in.pedalPct = 80;  // Sanfter Gas geben nach 3 s
  uint32_t t = 1000;
  eco::Tip got = eco::Tip::None;
  for (; t < 5000 && got == eco::Tip::None; t += 100) {
    in.nowMs = t;
    got = e.update(in);
  }
  TEST_ASSERT_EQUAL_INT((int)eco::Tip::HardPedal, (int)got);
  // weiter Vollgas: nach 8 s weg, dann 5 min nicht wieder
  for (; t < 200000; t += 100) {
    in.nowMs = t;
    got = e.update(in);
    if (t > 12000) TEST_ASSERT_EQUAL_INT((int)eco::Tip::None, (int)got);
  }
  // Fenster offen bzw. gerade getippt: nichts Neues
  eco::TipEngine q;
  in = driving(1000);
  in.pedalPct = 80;
  in.quiet = true;
  for (t = 1000; t < 10000; t += 100) {
    in.nowMs = t;
    TEST_ASSERT_EQUAL_INT((int)eco::Tip::None, (int)q.update(in));
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_score);
  RUN_TEST(test_brake_energy);
  RUN_TEST(test_power);
  RUN_TEST(test_shift_advice);
  RUN_TEST(test_gear_learning);
  RUN_TEST(test_display_gear);
  RUN_TEST(test_stable_k);
  RUN_TEST(test_gear_check);
  RUN_TEST(test_tip_coast);
  RUN_TEST(test_tip_idle);
  RUN_TEST(test_tip_cold);
  RUN_TEST(test_tip_gaps);
  return UNITY_END();
}

#include "../board_runner.h"
