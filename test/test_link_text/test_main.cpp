// Texte für Startbildschirm und Diagnose-Dialog (U Startbildschirm, U Menü Diagnose)
// link_text.h (mit <cmath>) vor unity.h, sonst ersetzt Unitys isnan-Makro std::isnan
#include "util/link_text.h"

#include <cstring>
#include <initializer_list>

#include <unity.h>

void setUp() {}
void tearDown() {}

static void support(LinkInfo& li, std::initializer_list<uint8_t> pids) {
  memset(li.supported, 0, sizeof(li.supported));
  for (uint8_t p : pids) li.supported[p / 8] |= static_cast<uint8_t>(1u << (p % 8));
  li.supportedKnown = true;
}

void test_supported_summary() {
  LinkInfo li;
  char text[48];
  linktext::supported(li, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("\xE2\x80\x93", text);  // noch unbekannt
  // vermuteter Modus: 11 Mess-PIDs, 0x20 ist nur der Verweis
  support(li, {0x01, 0x03, 0x04, 0x05, 0x06, 0x07, 0x0B, 0x0C, 0x0D, 0x0F, 0x11, 0x20});
  linktext::supported(li, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("11 (ohne MAF, 5E, Tank, Gaspedal)", text);
  support(li, {0x0B, 0x0C, 0x0D, 0x10, 0x2F, 0x49, 0x5E});
  linktext::supported(li, text, sizeof(text));
  TEST_ASSERT_EQUAL_STRING("7", text);
}

void test_fuel_source_order() {
  LinkInfo li;
  support(li, {0x0B, 0x0C, 0x10, 0x5E});
  TEST_ASSERT_EQUAL_STRING("Kraftstoffrate (5E)", linktext::fuelSource(li));
  support(li, {0x0B, 0x0C, 0x10});
  TEST_ASSERT_EQUAL_STRING("Luftmasse (MAF)", linktext::fuelSource(li));
  support(li, {0x0B, 0x0C});
  TEST_ASSERT_EQUAL_STRING("Saugrohrdruck", linktext::fuelSource(li));
  support(li, {0x0D});
  TEST_ASSERT_EQUAL_STRING("nicht verfügbar", linktext::fuelSource(li));
}

void test_start_steps() {
  LinkInfo li;
  linktext::Step st[linktext::STEP_COUNT];
  li.state = LinkState::Searching;
  linktext::startSteps(li, st);
  TEST_ASSERT_TRUE(st[0].kind == linktext::StepKind::Current);
  TEST_ASSERT_EQUAL_STRING("Suche Adapter \xE2\x80\xA6", st[0].text);
  TEST_ASSERT_TRUE(st[1].kind == linktext::StepKind::Hidden);

  strcpy(li.adapter, "vLinker MC");
  strcpy(li.protocol, "ISO 14230-4 KWP");
  strcpy(li.vehicle, "Renault Modus");
  li.state = LinkState::Running;
  linktext::startSteps(li, st);
  TEST_ASSERT_EQUAL_STRING("Verbunden mit vLinker MC", st[0].text);
  TEST_ASSERT_EQUAL_STRING("Protokoll: ISO 14230-4 KWP", st[1].text);
  TEST_ASSERT_EQUAL_STRING("Fahrzeug: Renault Modus", st[2].text);
  for (auto& s : st) TEST_ASSERT_TRUE(s.kind == linktext::StepKind::Done);

  // Auto antwortet nicht: Adapter erledigt, zweiter Schritt mit Fehler und Lösung
  li.state = LinkState::Waiting;
  li.error = LinkError::NoVehicle;
  linktext::startSteps(li, st);
  TEST_ASSERT_TRUE(st[0].kind == linktext::StepKind::Done);
  TEST_ASSERT_TRUE(st[1].kind == linktext::StepKind::Failed);
  TEST_ASSERT_EQUAL_STRING("Auto antwortet nicht", st[1].text);
  TEST_ASSERT_EQUAL_STRING("Zündung an?", linktext::hint(li.error));

  li.error = LinkError::AdapterNotFound;
  linktext::startSteps(li, st);
  TEST_ASSERT_TRUE(st[0].kind == linktext::StepKind::Failed);
  TEST_ASSERT_EQUAL_STRING("Zündung an? Handy-App des Adapters schließen.", linktext::hint(li.error));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_supported_summary);
  RUN_TEST(test_fuel_source_order);
  RUN_TEST(test_start_steps);
  return UNITY_END();
}
