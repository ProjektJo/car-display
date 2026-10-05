// Zahlenformat: Dezimalkomma, Tausenderpunkt ab 1000, "–" ohne Wert (M, Gestaltung)
#include <unity.h>

#include <cmath>

#include "util/format.h"

void setUp() {}
void tearDown() {}

static const char* f(float v, int d) {
  static char buf[32];
  fmt::number(buf, sizeof(buf), v, d);
  return buf;
}

void test_decimal_comma() {
  TEST_ASSERT_EQUAL_STRING("6,3", f(6.34f, 1));
  TEST_ASSERT_EQUAL_STRING("0,8", f(0.78f, 1));
  TEST_ASSERT_EQUAL_STRING("14,2", f(14.15f, 1));
  TEST_ASSERT_EQUAL_STRING("1,799", f(1.799f, 3));
}

void test_thousands_dot() {
  TEST_ASSERT_EQUAL_STRING("999", f(999, 0));
  TEST_ASSERT_EQUAL_STRING("2.500", f(2500, 0));
  TEST_ASSERT_EQUAL_STRING("15.000", f(15000, 0));
  TEST_ASSERT_EQUAL_STRING("1.234,5", f(1234.5f, 1));
  TEST_ASSERT_EQUAL_STRING("1.234.567", f(1234567, 0));
}

void test_rounding_carries_into_thousands() { TEST_ASSERT_EQUAL_STRING("1.000,0", f(999.96f, 1)); }

void test_negative_and_negative_zero() {
  TEST_ASSERT_EQUAL_STRING("-1,4", f(-1.4f, 1));
  TEST_ASSERT_EQUAL_STRING("0,0", f(-0.04f, 1));
  TEST_ASSERT_EQUAL_STRING("-1.200", f(-1200, 0));
}

void test_no_value() {
  TEST_ASSERT_EQUAL_STRING(fmt::NO_VALUE, f(NAN, 1));
  TEST_ASSERT_EQUAL_STRING(fmt::NO_VALUE, f(INFINITY, 0));
}

void test_small_buffer_is_cut_safely() {
  char buf[4];
  const size_t n = fmt::number(buf, sizeof(buf), 12345, 0);
  TEST_ASSERT_EQUAL_UINT32(3, n);
  TEST_ASSERT_EQUAL_STRING("12.", buf);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_decimal_comma);
  RUN_TEST(test_thousands_dot);
  RUN_TEST(test_rounding_carries_into_thousands);
  RUN_TEST(test_negative_and_negative_zero);
  RUN_TEST(test_no_value);
  RUN_TEST(test_small_buffer_is_cut_safely);
  return UNITY_END();
}

#include "../board_runner.h"
