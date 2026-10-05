// ELM327-Antworten zerlegen (A7, M Etappe 2): Fehlertexte, einzelne und mehrere PIDs, ISO-TP auf CAN,
// mehrere Steuergeräte, unterstützte PIDs, VIN, Bordspannung und Protokoll.
#include <unity.h>

#include <cstring>

#include "obd/elm_parser.h"

using namespace elmp;

void setUp() {}
void tearDown() {}

static int decode(const char* text, PidValue* out, int max) {
  Message msgs[8];
  const int n = parseMessages(text, msgs, 8);
  return decodeMode01(msgs, n, out, max);
}

void test_classify() {
  TEST_ASSERT_TRUE(classify("410C1AF8") == Reply::Data);
  TEST_ASSERT_TRUE(classify("41 0C 1A F8") == Reply::Data);
  TEST_ASSERT_TRUE(classify("SEARCHING...\r410C1AF8") == Reply::Data);
  TEST_ASSERT_TRUE(classify("00A\r0:410C1AF80D32\r1:0B1E1122000000") == Reply::Data);
  TEST_ASSERT_TRUE(classify("NO DATA") == Reply::NoData);
  TEST_ASSERT_TRUE(classify("SEARCHING...\rUNABLE TO CONNECT") == Reply::Unable);
  TEST_ASSERT_TRUE(classify("BUS INIT: ...ERROR") == Reply::Unable);
  TEST_ASSERT_TRUE(classify("?") == Reply::Error);
  TEST_ASSERT_TRUE(classify("CAN ERROR") == Reply::Error);
  TEST_ASSERT_TRUE(classify("STOPPED") == Reply::Error);
  TEST_ASSERT_TRUE(classify("OK") == Reply::Ok);
  TEST_ASSERT_TRUE(classify("ELM327 v2.2") == Reply::Ok);
  TEST_ASSERT_TRUE(classify("\r\r") == Reply::Empty);
}

void test_single_pid() {
  PidValue v[4];
  TEST_ASSERT_EQUAL(1, decode("410C1AF8", v, 4));
  TEST_ASSERT_EQUAL_HEX8(0x0C, v[0].pid);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1726.0f, v[0].value);  // (0x1A·256 + 0xF8) / 4
  TEST_ASSERT_EQUAL(1, decode("41 05 7B", v, 4));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 83.0f, v[0].value);  // 0x7B − 40 °C
  TEST_ASSERT_EQUAL(1, decode("SEARCHING...\r410D32", v, 4));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, v[0].value);
}

void test_multi_pid_can_isotp() {
  // 4 PIDs in einer Anfrage (010C0D0B11), Antwort über zwei CAN-Rahmen, Füllbytes am Ende
  PidValue v[8];
  TEST_ASSERT_EQUAL(4, decode("00A\r0:410C1AF80D32\r1:0B1E1122000000", v, 8));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1726.0f, v[0].value);
  TEST_ASSERT_EQUAL_HEX8(0x0D, v[1].pid);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, v[1].value);
  TEST_ASSERT_EQUAL_HEX8(0x0B, v[2].pid);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 30.0f, v[2].value);
  TEST_ASSERT_EQUAL_HEX8(0x11, v[3].pid);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 13.33f, v[3].value);  // 0x22 · 100 / 255
}

void test_multi_pid_single_frame() {
  PidValue v[8];
  TEST_ASSERT_EQUAL(2, decode("410D320C1AF8", v, 8));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, v[0].value);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1726.0f, v[1].value);
}

void test_two_ecus_first_wins() {
  PidValue v[4];
  TEST_ASSERT_EQUAL(1, decode("410C1AF8\r410C0000", v, 4));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1726.0f, v[0].value);
}

void test_errors_give_no_values() {
  PidValue v[4];
  TEST_ASSERT_EQUAL(0, decode("NO DATA", v, 4));
  TEST_ASSERT_EQUAL(0, decode("?", v, 4));
  TEST_ASSERT_EQUAL(0, decode("410C1A", v, 4));  // abgeschnitten
}

void test_formulas() {
  PidValue v[4];
  decode("410183076504", v, 4);  // MIL an, 3 Fehlercodes
  TEST_ASSERT_EQUAL_FLOAT(1.0f, v[0].value);
  TEST_ASSERT_EQUAL_FLOAT(3.0f, v[0].value2);
  decode("410680", v, 4);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, v[0].value);  // STFT 128 = 0 %
  decode("410790", v, 4);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.5f, v[0].value);  // LTFT 0x90 = +12,5 %
  decode("41100190", v, 4);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.0f, v[0].value);  // MAF 400 / 100 g/s
  decode("415E0050", v, 4);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.0f, v[0].value);  // 80 / 20 l/h
  decode("41448000", v, 4);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, v[0].value);  // Soll-Lambda 1,0
  decode("410304 00", v, 4);
  TEST_ASSERT_EQUAL_FLOAT(4.0f, v[0].value);  // Schub
}

void test_supported_pids() {
  Message m[4];
  int n = parseMessages("4100BE1FA813", m, 4);
  uint32_t mask = 0;
  TEST_ASSERT_TRUE(decodeSupported(m, n, 0x00, mask));
  TEST_ASSERT_EQUAL_HEX32(0xBE1FA813, mask);
  uint8_t bits[32] = {};
  markSupported(bits, 0x00, mask);
  TEST_ASSERT_TRUE(bits[0x01 / 8] & (1 << (0x01 % 8)));
  TEST_ASSERT_FALSE(bits[0x02 / 8] & (1 << (0x02 % 8)));
  TEST_ASSERT_TRUE(bits[0x0C / 8] & (1 << (0x0C % 8)));
  TEST_ASSERT_TRUE(bits[0x20 / 8] & (1 << (0x20 % 8)));  // Verweis auf 0120
  // 0xBE1FA813 hat 17 gesetzte Bits, davon ist 0x20 nur der Verweis
  TEST_ASSERT_EQUAL(16, countSupported(bits));

  // zwei Steuergeräte: Bits zusammenfassen
  n = parseMessages("4100BE1FA813\r410098180001", m, 4);
  TEST_ASSERT_TRUE(decodeSupported(m, n, 0x00, mask));
  TEST_ASSERT_EQUAL_HEX32(0xBE1FA813u | 0x98180001u, mask);
  // falsche Liste
  TEST_ASSERT_FALSE(decodeSupported(m, n, 0x20, mask));
}

void test_vin_can_and_kwp() {
  Message m[8];
  char vin[18];
  int n = parseMessages("014\r0:490201314731\r1:4A433534343452\r2:37323532333637", m, 8);
  TEST_ASSERT_TRUE(decodeVin(m, n, vin, sizeof(vin)));
  TEST_ASSERT_EQUAL_STRING("1G1JC5444R7252367", vin);

  n = parseMessages("49020100000031\r49020247314A43\r49020335343434\r49020452373235\r49020532333637", m, 8);
  TEST_ASSERT_TRUE(decodeVin(m, n, vin, sizeof(vin)));
  TEST_ASSERT_EQUAL_STRING("1G1JC5444R7252367", vin);

  n = parseMessages("NO DATA", m, 8);
  TEST_ASSERT_FALSE(decodeVin(m, n, vin, sizeof(vin)));
}

void test_voltage_and_protocol() {
  float v = 0;
  TEST_ASSERT_TRUE(parseVoltage("12.6V", v));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.6f, v);
  TEST_ASSERT_TRUE(parseVoltage("14,2V", v));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 14.2f, v);
  TEST_ASSERT_FALSE(parseVoltage("?", v));

  TEST_ASSERT_EQUAL(6, parseProtocolNumber("A6"));
  TEST_ASSERT_EQUAL(5, parseProtocolNumber("5"));
  TEST_ASSERT_EQUAL(-1, parseProtocolNumber("?"));
  TEST_ASSERT_TRUE(protocolIsCan(6));
  TEST_ASSERT_FALSE(protocolIsCan(5));
  TEST_ASSERT_EQUAL_STRING("ISO 14230-4 KWP", protocolName(5));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_classify);
  RUN_TEST(test_single_pid);
  RUN_TEST(test_multi_pid_can_isotp);
  RUN_TEST(test_multi_pid_single_frame);
  RUN_TEST(test_two_ecus_first_wins);
  RUN_TEST(test_errors_give_no_values);
  RUN_TEST(test_formulas);
  RUN_TEST(test_supported_pids);
  RUN_TEST(test_vin_can_and_kwp);
  RUN_TEST(test_voltage_and_protocol);
  return UNITY_END();
}

#include "../board_runner.h"
