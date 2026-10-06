// Fehlercodes (A11): Mode 03/07 zerlegen, Format, deutscher Klartext
#include "obd/dtc.h"

#include <cstring>
#include <initializer_list>

#include <unity.h>

void setUp() {}
void tearDown() {}

static elmp::Message msg(std::initializer_list<uint8_t> bytes) {
  elmp::Message m;
  m.len = 0;
  for (uint8_t b : bytes) m.data[m.len++] = b;
  return m;
}

// CAN: Kennbyte 43, Anzahl, dann die Codes
void test_decode_can() {
  elmp::Message m[1] = {msg({0x43, 0x02, 0x01, 0x71, 0x03, 0x00})};
  dtc::Code c[8];
  TEST_ASSERT_EQUAL_INT(2, dtc::decode(m, 1, 0x03, c, 8));
  TEST_ASSERT_EQUAL_HEX16(0x0171, c[0]);
  TEST_ASSERT_EQUAL_HEX16(0x0300, c[1]);
  elmp::Message none[1] = {msg({0x43, 0x00})};
  TEST_ASSERT_EQUAL_INT(0, dtc::decode(none, 1, 0x03, c, 8));
}

// KWP: bis zu drei Codes je Nachricht, mit 0000 aufgefüllt; zwei Steuergeräte, doppelte zählen einmal
void test_decode_kwp() {
  elmp::Message m[2] = {msg({0x43, 0x01, 0x71, 0x04, 0x20, 0x00, 0x00}), msg({0x43, 0x01, 0x71, 0x00, 0x00, 0x00, 0x00})};
  dtc::Code c[8];
  TEST_ASSERT_EQUAL_INT(2, dtc::decode(m, 2, 0x03, c, 8));
  TEST_ASSERT_EQUAL_HEX16(0x0171, c[0]);
  TEST_ASSERT_EQUAL_HEX16(0x0420, c[1]);
  // vorläufig: 47
  elmp::Message p[1] = {msg({0x47, 0x01, 0x01, 0x33})};
  TEST_ASSERT_EQUAL_INT(1, dtc::decode(p, 1, 0x07, c, 8));
  TEST_ASSERT_EQUAL_INT(0, dtc::decode(p, 1, 0x03, c, 8));
}

void test_format_and_text() {
  char t[8];
  dtc::format(0x0171, t, sizeof(t));
  TEST_ASSERT_EQUAL_STRING("P0171", t);
  dtc::format(0xC155, t, sizeof(t));  // U0155
  TEST_ASSERT_EQUAL_STRING("U0155", t);
  dtc::format(0x1234, t, sizeof(t));  // P1234
  TEST_ASSERT_EQUAL_STRING("P1234", t);
  TEST_ASSERT_EQUAL_STRING("Gemisch zu mager (Bank 1)", dtc::text(0x0171));
  TEST_ASSERT_EQUAL_STRING("Herstellerspezifischer Code", dtc::text(0x1234));
  TEST_ASSERT_TRUE(dtc::tableSize() >= 300);
  // Tabelle sortiert (Binärsuche) und jeder Eintrag findbar
  TEST_ASSERT_TRUE(strstr(dtc::text(0x0420), "Katalysator") != nullptr);
  TEST_ASSERT_TRUE(strstr(dtc::text(0x0010), "Nockenwellenversteller") != nullptr);
  TEST_ASSERT_TRUE(strstr(dtc::text(0x0850), "Park") != nullptr);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_decode_can);
  RUN_TEST(test_decode_kwp);
  RUN_TEST(test_format_and_text);
  return UNITY_END();
}

#include "../board_runner.h"
