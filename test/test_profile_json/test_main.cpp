// Profil als JSON (A6) und Prüfsumme der Speicherdateien (A8)
#include "storage/profile_json.h"

#include <cstring>

#include <unity.h>

#include "calc/persist.h"
#include "util/crc32.h"

void setUp() {}
void tearDown() {}

void test_roundtrip() {
  Profile p;
  p.id = 3;
  snprintf(p.name, sizeof(p.name), "%s", "Kleiner Flitzer");
  snprintf(p.vin, sizeof(p.vin), "%s", "VF1JP0A0H12345678");
  const uint8_t pids[] = {0x01, 0x03, 0x0C, 0x0D, 0x20, 0x2F, 0x40, 0x49, 0xFF};
  for (uint8_t pid : pids) p.supported[pid / 8] |= static_cast<uint8_t>(1u << (pid % 8));
  p.protocol = 5;
  p.fuel = FuelType::Diesel;
  p.displacementL = 1.5f;
  p.tankL = 45;
  p.fuelCal = 1.0483f;
  p.gears[0] = 7.9f;
  p.gears[1] = 14.2f;
  p.gearCount = 2;
  p.body = 2;
  p.applyDerivedDefaults();

  char json[1536];
  TEST_ASSERT_GREATER_THAN(0, profileToJson(p, json, sizeof(json)));
  // PID-Liste wie die Antwort auf 0100: 01, 03, 0C, 0D, 20 -> A0 18 00 01
  TEST_ASSERT_NOT_NULL(strstr(json, "\"supported_pids\": \"A0180001"));

  Profile q;
  q.id = 3;
  TEST_ASSERT_TRUE(profileFromJson(json, q));
  TEST_ASSERT_EQUAL_STRING(p.name, q.name);
  TEST_ASSERT_EQUAL_STRING(p.vin, q.vin);
  TEST_ASSERT_EQUAL_MEMORY(p.supported, q.supported, sizeof(p.supported));
  TEST_ASSERT_EQUAL_INT8(5, q.protocol);
  TEST_ASSERT_TRUE(q.fuel == FuelType::Diesel);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.5f, q.displacementL);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 45.0f, q.tankL);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0483f, q.fuelCal);
  TEST_ASSERT_EQUAL_UINT8(2, q.gearCount);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 14.2f, q.gears[1]);
  TEST_ASSERT_EQUAL_UINT8(2, q.body);
  TEST_ASSERT_EQUAL_UINT16(p.shiftRpm, q.shiftRpm);
  TEST_ASSERT_EQUAL_UINT16(p.powerKw, q.powerKw);
}

void test_defaults_and_limits() {
  Profile q;
  q.id = 1;
  // fehlende Felder bekommen Standardwerte, unsinnige werden begrenzt
  TEST_ASSERT_TRUE(profileFromJson("{\"name\":\"X\",\"tank_l\":0,\"fuel_cal\":5}", q));
  TEST_ASSERT_EQUAL_UINT8(1, q.id);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, cfg::DEFAULT_DISPLACEMENT_L, q.displacementL);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, cfg::TANK_MIN_L, q.tankL);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, cfg::FUEL_CAL_MAX, q.fuelCal);
  TEST_ASSERT_EQUAL_INT8(-1, q.protocol);
  TEST_ASSERT_FALSE(q.pidSupported(0x0C));
  // kaputte Datei oder ohne Namen: nicht übernehmen
  Profile r;
  snprintf(r.name, sizeof(r.name), "%s", "alt");
  TEST_ASSERT_FALSE(profileFromJson("{\"name\":", r));
  TEST_ASSERT_FALSE(profileFromJson("{\"tank_l\":40}", r));
  TEST_ASSERT_EQUAL_STRING("alt", r.name);
}

void test_crc32() {
  // Prüfwert der Norm: "123456789" -> CBF43926
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, crc32("123456789", 9));
  // Fortsetzen über zwei Teile ergibt dasselbe
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, crc32("6789", 4, crc32("12345", 5)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_roundtrip);
  RUN_TEST(test_defaults_and_limits);
  RUN_TEST(test_crc32);
  return UNITY_END();
}

#include "../board_runner.h"
