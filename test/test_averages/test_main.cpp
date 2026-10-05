// Strecken-Ringpuffer und Tank-Schnitt (M Prüfwerte, A7)
#include "calc/averages.h"

#include <unity.h>

#include "config.h"

void setUp() {}
void tearDown() {}

// in Schritten von 10 m fahren, l100 = Verbrauch auf dieser Strecke
static void drive(avg::DistanceRing& r, float km, float l100) {
  const float stepM = 10.0f;
  for (int i = 0; i < (int)(km * 1000 / stepM + 0.5f); i++) r.add(stepM, stepM * l100 / 100.0f);
}

// 12 km gefahren, die ersten 2 km mit 10 l/100 km, dann 5 l/100 km -> Ø 10 km = 5,0
void test_ring_10km() {
  avg::DistanceRing r;
  r.init(cfg::AVG10_SLOTS, cfg::AVG10_SLOT_M);
  drive(r, 2.0f, 10.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, r.l100());
  drive(r, 10.0f, 5.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, r.l100());
  TEST_ASSERT_TRUE(r.sumM() <= 10000.5f);
}

void test_ring_min_distance_and_idle() {
  avg::DistanceRing r;
  r.init(cfg::AVG1_SLOTS, cfg::AVG1_SLOT_M);
  drive(r, 0.05f, 6.0f);
  TEST_ASSERT_FLOAT_IS_NAN(r.l100());  // unter 10 % der Fensterlänge
  drive(r, 0.2f, 6.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 6.0f, r.l100());
  r.add(0, 5.0f);  // Leerlauf: Sprit ohne Strecke zählt mit
  TEST_ASSERT_TRUE(r.l100() > 6.0f);
}

void test_ring_large_step() {
  // Ein Schritt über mehrere Abschnitte wird anteilig verteilt
  avg::DistanceRing r;
  r.init(cfg::AVG1_SLOTS, cfg::AVG1_SLOT_M);
  r.add(175.0f, 17.5f);  // 10 l/100 km
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 175.0f, r.sumM());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, r.l100());
}

// 20 km seit Tanken, vorige Füllung 6,5 l/100 km -> 6,5; ab 30 km der eigene Wert
void test_tank_average() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.5f, avg::tankL100(20.0f, 1.0f, 6.5f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, avg::tankL100(30.0f, 1.5f, 6.5f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, avg::tankL100(20.0f, 1.0f, NAN));  // ohne vorige Füllung
}

void test_fill_history() {
  avg::FillHistory h;
  h.clear();
  TEST_ASSERT_FLOAT_IS_NAN(h.l100());
  for (int i = 0; i < 7; i++) h.push(500.0f, i < 2 ? 50.0f : 31.0f);  // die ersten zwei fallen heraus
  TEST_ASSERT_EQUAL_UINT8(5, h.count);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.2f, h.l100());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_ring_10km);
  RUN_TEST(test_ring_min_distance_and_idle);
  RUN_TEST(test_ring_large_step);
  RUN_TEST(test_tank_average);
  RUN_TEST(test_fill_history);
  return UNITY_END();
}
