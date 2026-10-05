// Plausibilität der simulierten Fahrt: die Daten müssen später alle Seiten und Rechnungen bedienen
#include <unity.h>

#include <cmath>

#include "sim/simulator.h"

void setUp() {}
void tearDown() {}

constexpr float DT = 0.125f;

// Speed-Density wie Architektur 7, mit den Konstanten der Simulation
static float speedDensityLph(const SimOutput& o) {
  const float airGs = o.mapKpa * DriveSim::DISPLACEMENT_L * o.rpm / 120.0f * DriveSim::VE / (0.28705f * (o.iatC + 273.15f));
  const float fuelGs = airGs / DriveSim::AFR * (1.0f + (o.stftPct + o.ltftPct) / 100.0f);
  return fuelGs * 3600.0f / DriveSim::DENSITY_G_PER_L;
}

void test_values_stay_plausible_and_events_happen() {
  DriveSim sim(7);
  bool sawOverrun = false, sawEngineOff = false, sawSail = false, sawCoastBrake = false, sawMil = false;
  float minCoolant = 1e9f, maxSpeed = 0;
  int sdChecked = 0;
  for (int i = 0; i < static_cast<int>(700 / DT); i++) {
    sim.step(DT);
    const SimOutput& o = sim.out();
    TEST_ASSERT_TRUE(o.speedKmh >= 0 && o.speedKmh < 130);
    TEST_ASSERT_TRUE(o.rpm >= 0 && o.rpm < 6500);
    TEST_ASSERT_TRUE(o.pedalPct >= 0 && o.pedalPct <= 100);
    TEST_ASSERT_TRUE(o.fuelLevelPct >= 0 && o.fuelLevelPct <= 100);
    minCoolant = std::fmin(minCoolant, o.coolantC);
    maxSpeed = std::fmax(maxSpeed, o.speedKmh);
    if (o.fuelSys == 4) {
      sawOverrun = true;
      TEST_ASSERT_EQUAL_FLOAT(0.0f, o.trueLph);  // Schubabschaltung = exakt 0 (A7)
      TEST_ASSERT_TRUE(o.rpm > 1000 && o.speedKmh > 14);
    }
    if (!o.engineOn) {
      sawEngineOff = true;
      TEST_ASSERT_EQUAL_FLOAT(0.0f, o.rpm);
    }
    if (sim.mode() == DriveSim::Mode::Sail && o.speedKmh > 20) sawSail = true;
    if (sim.mode() == DriveSim::Mode::CoastBrake && o.speedKmh > 20) sawCoastBrake = true;
    if (o.mil) {
      sawMil = true;
      TEST_ASSERT_EQUAL_HEX16(0x0171, o.dtc);
    }
    // Saugrohrdruck passt zum wahren Verbrauch (solange nicht an der Grenze 20/99 kPa)
    if (o.engineOn && o.fuelSys == 2 && o.mapKpa > 21 && o.mapKpa < 98) {
      TEST_ASSERT_FLOAT_WITHIN(0.02f * o.trueLph + 0.01f, o.trueLph, speedDensityLph(o));
      sdChecked++;
    }
  }
  TEST_ASSERT_TRUE(sawOverrun);
  TEST_ASSERT_TRUE(sawEngineOff);
  TEST_ASSERT_TRUE(sawSail);
  TEST_ASSERT_TRUE(sawCoastBrake);
  TEST_ASSERT_TRUE(sawMil);
  TEST_ASSERT_TRUE(sdChecked > 1000);
  TEST_ASSERT_TRUE(minCoolant < 20);       // Kaltstart
  TEST_ASSERT_TRUE(sim.out().coolantC > 60);
  TEST_ASSERT_TRUE(maxSpeed > 95);         // Landstraße
}

void test_refuel_fills_tank() {
  DriveSim sim(3);
  float levelBefore = -1, levelAfter = -1;
  bool wasOff = false;
  for (int i = 0; i < static_cast<int>(700 / DT); i++) {
    sim.step(DT);
    const SimOutput& o = sim.out();
    if (!o.engineOn && !wasOff) levelBefore = o.fuelLevelPct;
    if (o.engineOn && wasOff) levelAfter = o.fuelLevelPct;
    wasOff = !o.engineOn;
  }
  TEST_ASSERT_TRUE(levelBefore > 0 && levelBefore < 60);
  TEST_ASSERT_TRUE(levelAfter > 97);
}

void test_sprint_reaches_100_with_shift_pauses() {
  DriveSim sim(5);
  sim.requestSprint();
  float tStart = -1, t100 = -1;
  int shiftPauses = 0;
  bool inPause = false;
  for (int i = 0; i < static_cast<int>(200 / DT) && t100 < 0; i++) {
    sim.step(DT);
    const SimOutput& o = sim.out();
    if (sim.mode() != DriveSim::Mode::Sprint) {
      if (tStart >= 0) t100 = sim.timeS();
      continue;
    }
    if (tStart < 0) tStart = sim.timeS();
    TEST_ASSERT_TRUE(o.rpm <= 5800);
    const bool pause = o.speedKmh > 3 && o.pedalPct < 50;
    if (pause && !inPause) shiftPauses++;
    inPause = pause;
  }
  TEST_ASSERT_TRUE(tStart >= 0);
  TEST_ASSERT_TRUE(t100 > 0);
  const float dur = t100 - tStart;
  TEST_ASSERT_TRUE(dur > 10 && dur < 20);  // ca. 13 s plus Hochdrehen
  TEST_ASSERT_TRUE(shiftPauses >= 2);      // Schaltpausen 1→2, 2→3 (…)
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_values_stay_plausible_and_events_happen);
  RUN_TEST(test_refuel_fills_tank);
  RUN_TEST(test_sprint_reaches_100_with_shift_pauses);
  return UNITY_END();
}

#include "../board_runner.h"
