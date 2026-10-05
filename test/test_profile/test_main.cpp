// Profil erkennen: erst VIN, sonst PID-Liste und Protokoll (A6, Jos Entscheidung vom 5. Oktober)
#include "storage/profile_match.h"

#include <cstdio>
#include <cstring>
#include <initializer_list>

#include <unity.h>

void setUp() {}
void tearDown() {}

static void bitsOf(uint8_t bits[32], std::initializer_list<uint8_t> pids) {
  memset(bits, 0, 32);
  for (uint8_t p : pids) bits[p / 8] |= static_cast<uint8_t>(1u << (p % 8));
}

static ProfileSummary make(uint8_t id, const char* vin, std::initializer_list<uint8_t> pids, int8_t proto) {
  ProfileSummary s;
  s.id = id;
  snprintf(s.vin, sizeof(s.vin), "%s", vin);
  bitsOf(s.supported, pids);
  s.protocol = proto;
  return s;
}

void test_match() {
  ProfileSummary list[3] = {
      make(1, "", {0x0B, 0x0C, 0x0D}, 5),                 // Modus: keine VIN, KWP
      make(2, "WVWZZZ1KZAW000001", {0x0C, 0x0D, 0x10}, 6),  // Golf mit VIN
      make(3, "", {0x0C, 0x0D, 0x10, 0x5E}, 6),
  };
  uint8_t bits[32];

  // PID-Liste und Protokoll gleich, keine VIN
  bitsOf(bits, {0x0B, 0x0C, 0x0D});
  ProfileMatch m = matchProfile(list, 3, "", bits, 5);
  TEST_ASSERT_TRUE(m.kind == ProfileMatch::Kind::One);
  TEST_ASSERT_EQUAL_UINT8(1, m.id);
  TEST_ASSERT_FALSE(m.learnVin);

  // anderes Protokoll -> kein Treffer, Frage "Welches Fahrzeug?"
  TEST_ASSERT_TRUE(matchProfile(list, 3, "", bits, 6).kind == ProfileMatch::Kind::None);

  // VIN gewinnt, auch wenn die PID-Liste anders ist
  m = matchProfile(list, 3, "WVWZZZ1KZAW000001", bits, 5);
  TEST_ASSERT_TRUE(m.kind == ProfileMatch::Kind::One);
  TEST_ASSERT_EQUAL_UINT8(2, m.id);

  // Profil mit VIN passt nicht zu einem Auto ohne VIN, auch bei gleicher PID-Liste
  bitsOf(bits, {0x0C, 0x0D, 0x10});
  TEST_ASSERT_TRUE(matchProfile(list, 3, "", bits, 6).kind == ProfileMatch::Kind::None);

  // Auto liefert eine neue VIN, PID-Liste passt zu einem Profil ohne VIN: VIN merken
  bitsOf(bits, {0x0C, 0x0D, 0x10, 0x5E});
  m = matchProfile(list, 3, "VF1JP0A0H12345678", bits, 6);
  TEST_ASSERT_TRUE(m.kind == ProfileMatch::Kind::One);
  TEST_ASSERT_EQUAL_UINT8(3, m.id);
  TEST_ASSERT_TRUE(m.learnVin);

  // zwei gleiche Profile -> fragen
  list[2] = make(3, "", {0x0B, 0x0C, 0x0D}, 5);
  bitsOf(bits, {0x0B, 0x0C, 0x0D});
  TEST_ASSERT_TRUE(matchProfile(list, 3, "", bits, 5).kind == ProfileMatch::Kind::Many);

  // keine Profile
  TEST_ASSERT_TRUE(matchProfile(list, 0, "", bits, 5).kind == ProfileMatch::Kind::None);
}

// Welche Profile geben ein Auto frei, wenn es einem anderen zugeordnet wird
void test_fits_car() {
  uint8_t bits[32];
  bitsOf(bits, {0x0B, 0x0C, 0x0D});
  const ProfileSummary noVin = make(1, "", {0x0B, 0x0C, 0x0D}, 5);
  const ProfileSummary withVin = make(2, "VF1JP0A0H12345678", {0x0B, 0x0C, 0x0D}, 5);
  const ProfileSummary other = make(3, "", {0x0C, 0x0D}, 5);
  TEST_ASSERT_TRUE(profileFitsCar(noVin, "", bits, 5));
  TEST_ASSERT_TRUE(profileFitsCar(noVin, "VF1JP0A0H12345678", bits, -1));  // Protokoll unbekannt
  TEST_ASSERT_TRUE(profileFitsCar(withVin, "VF1JP0A0H12345678", bits, 6));  // VIN gleich
  TEST_ASSERT_FALSE(profileFitsCar(withVin, "", bits, 5));                  // Profil mit VIN, Auto ohne
  TEST_ASSERT_FALSE(profileFitsCar(other, "", bits, 5));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_match);
  RUN_TEST(test_fits_car);
  return UNITY_END();
}
