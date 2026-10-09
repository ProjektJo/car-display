// Fahrtende ohne Uhr (M Prüfwerte, A8)
#include "calc/trip.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

// Start ≥ 60 °C oder höchstens 5 °C kälter als beim Abstellen -> Fahrt läuft weiter, sonst neue
void test_trip_end_by_coolant() {
  TEST_ASSERT_TRUE(trip::continues(88.0f, 85.0f));
  TEST_ASSERT_TRUE(trip::continues(88.0f, 60.0f));   // noch 60 °C: weiter
  TEST_ASSERT_FALSE(trip::continues(88.0f, 55.0f));  // abgekühlt: neue Fahrt
  TEST_ASSERT_TRUE(trip::continues(45.0f, 40.0f));   // kalt abgestellt, genau 5 °C kälter: weiter
  TEST_ASSERT_FALSE(trip::continues(45.0f, 39.0f));
  TEST_ASSERT_TRUE(trip::continues(NAN, 85.0f));     // ohne Wert beim Abstellen: warm genügt
  TEST_ASSERT_FALSE(trip::continues(NAN, 50.0f));
  TEST_ASSERT_FALSE(trip::continues(88.0f, NAN));
}

void test_record() {
  trip::TripState t;
  trip::start(t, 7);
  t.km = 12.5f;
  t.liters = 0.8f;
  trip::TripRecord r = trip::toRecord(t, 2);
  TEST_ASSERT_EQUAL_UINT16(7, r.number);
  TEST_ASSERT_EQUAL_UINT8(2, r.profileId);
  TEST_ASSERT_FLOAT_IS_NAN(r.cost);  // ohne Preis
  t.cost = 1.4f;
  t.costKnown = 1;
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.4f, trip::toRecord(t, 2).cost);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_trip_end_by_coolant);
  RUN_TEST(test_record);
  return UNITY_END();
}

#include "../board_runner.h"
