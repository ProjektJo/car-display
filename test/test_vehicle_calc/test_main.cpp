// Rechnung eines Profils im Zusammenspiel (A7/A8): simulierte Fahrt gegen den wahren Verbrauch,
// Tanken mit Kalibrierung, Fahrt weiter bzw. neu nach dem Start, Fahrtende ohne Motor, Speichern.
#include "calc/vehicle_calc.h"

#include <cmath>
#include <cstring>
#include <initializer_list>

#include <unity.h>

#include "sim/simulator.h"

void setUp() {}
void tearDown() {}

static void support(LinkInfo& li, std::initializer_list<uint8_t> pids) {
  memset(li.supported, 0, sizeof(li.supported));
  for (uint8_t p : pids) li.supported[p / 8] |= static_cast<uint8_t>(1u << (p % 8));
  li.supportedKnown = true;
}

static Profile simProfile(uint8_t id = 1) {
  Profile p;
  p.id = id;
  snprintf(p.name, sizeof(p.name), "Test");
  p.displacementL = DriveSim::DISPLACEMENT_L;
  p.ve = DriveSim::VE;
  p.tankL = DriveSim::TANK_L;
  return p;
}

// Simulierte Fahrt: berechnete Liter liegen nahe am wahren Verbrauch der Simulation
void test_sim_drive_liters() {
  DriveSim sim;
  CarState s;
  support(s.link, {0x01, 0x03, 0x04, 0x05, 0x06, 0x07, 0x0B, 0x0C, 0x0D, 0x0F, 0x11, 0x2F, 0x49});
  VehicleCalc calc;
  calc.load(simProfile(), nullptr);
  const float dt = 0.125f;
  double trueL = 0, trueKm = 0;
  uint32_t now = 1;
  for (int i = 0; i < (int)(40 * 60 / dt); i++) {  // 40 min
    sim.step(dt);
    now += 125;
    const SimOutput& o = sim.out();
    s.speed.set(o.speedKmh, now);
    s.rpm.set(o.rpm, now);
    s.map.set(o.mapKpa, now);
    s.throttle.set(o.throttlePct, now);
    s.iat.set(o.iatC, now);
    s.stft.set(o.stftPct, now);
    s.ltft.set(o.ltftPct, now);
    s.fuelSys.set(o.fuelSys, now);
    s.coolant.set(o.coolantC, now);
    s.fuelLevel.set(o.fuelLevelPct, now);
    calc.step(s, now, dt);
    trueL += o.trueLph * dt / 3600.0;
    trueKm += o.speedKmh * dt / 3600.0;
  }
  const PersistState& st = calc.state();
  TEST_ASSERT_TRUE(trueKm > 10);
  TEST_ASSERT_FLOAT_WITHIN(0.01 * trueKm, trueKm, st.totalKm);
  TEST_ASSERT_FLOAT_WITHIN(0.03 * trueL, trueL, st.totalL);  // ± 3 %
  TEST_ASSERT_TRUE(st.trip.active);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, (float)st.totalKm, (float)st.trip.km);
  TEST_ASSERT_FLOAT_IS_NOT_NAN(calc.out().avg10);
  TEST_ASSERT_FLOAT_IS_NOT_NAN(calc.out().tankL);    // aus 0x2F
  TEST_ASSERT_FLOAT_IS_NOT_NAN(calc.out().rangeKm);
}

// Gleichmäßige Fahrt: 50 km/h, fester Saugrohrdruck. Liefert l/h dieser Fahrt.
static float cruise(VehicleCalc& calc, CarState& s, uint32_t& now, float km) {
  const float dt = 0.1f;
  float lph = 0;
  for (int i = 0; i < (int)(km / 50.0f * 3600.0f / dt + 0.5f); i++) {
    now += 100;
    s.speed.set(50, now);
    s.rpm.set(2000, now);
    s.map.set(45, now);
    s.iat.set(25, now);
    s.fuelSys.set(2, now);
    s.coolant.set(90, now);
    calc.step(s, now, dt);
    lph = calc.out().instLph;
  }
  return lph;
}

// Tanken: Tankmodell, Mischpreis, Kalibrierung zwischen zwei Vollbetankungen
void test_refuel_and_calibration() {
  CarState s;
  support(s.link, {0x03, 0x05, 0x0B, 0x0C, 0x0D, 0x0F});  // ohne 0x2F: Tankmodell
  VehicleCalc calc;
  calc.load(simProfile(), nullptr);
  uint32_t now = 1;
  cruise(calc, s, now, 1.0f);
  TEST_ASSERT_FLOAT_IS_NAN(calc.out().tankL);  // vor dem ersten Tanken unbekannt

  bool cal = false;
  calc.refuel(30.0f, 1.799f, true, trip::FillSource::Entered, cal);
  TEST_ASSERT_FALSE(cal);  // erste Vollbetankung startet nur den Zeitraum
  cruise(calc, s, now, 0.1f);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 49.0f, calc.out().tankPhysL);
  // angezeigt wird nur der nutzbare Teil: Reserve pauschal 4 % von 49 l = 1,96 l
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 49.0f - 1.96f, calc.out().tankL);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.799f, calc.out().mixPrice);

  cruise(calc, s, now, 400.0f);
  const float computed = (float)calc.state().calComputedL;
  const float rest = calc.out().tankPhysL;  // physisch (Mischpreis rechnet mit dem echten Inhalt)
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 49.0f - computed, rest);
  // getankt = berechnet · 40/36,4 -> fuel_cal · 1,0483
  calc.refuel(computed * 40.0f / 36.4f, 1.699f, true, trip::FillSource::Entered, cal);
  TEST_ASSERT_TRUE(cal);
  TEST_ASSERT_TRUE(calc.takeProfileChanged());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0483f, calc.profile().fuelCal);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, (float)calc.state().fillKm);
  TEST_ASSERT_FLOAT_IS_NOT_NAN(calc.state().prevFillL100);
  // Mischpreis aus Rest und neuem Sprit
  TEST_ASSERT_FLOAT_WITHIN(0.0005f, (rest * 1.799f + computed * 40.0f / 36.4f * 1.699f) / (rest + computed * 40.0f / 36.4f),
                           calc.state().mixPrice);
  TEST_ASSERT_TRUE(calc.saveDue(now));
}

// Nach dem Start: warm abgestellt und kaum abgekühlt = dieselbe Fahrt, sonst neue (A8)
void test_trip_continues_after_restart() {
  CarState s;
  support(s.link, {0x05, 0x0B, 0x0C, 0x0D});
  VehicleCalc calc;
  calc.load(simProfile(), nullptr);
  uint32_t now = 1;
  cruise(calc, s, now, 5.0f);
  PersistState saved = calc.state();
  const uint16_t number = saved.trip.number;
  saved.coolantLastC = 88;

  VehicleCalc again;
  again.load(simProfile(), &saved);
  s.coolant.set(85, now);
  s.rpm.set(800, now);
  again.step(s, now, 0.1f);
  TEST_ASSERT_EQUAL_UINT16(number, again.state().trip.number);
  trip::TripRecord rec;
  TEST_ASSERT_FALSE(again.takeTripRecord(rec));

  VehicleCalc cold;
  cold.load(simProfile(), &saved);
  s.coolant.set(50, now);  // unter 60 °C und mehr als 5 °C kälter: neue Fahrt
  cold.step(s, now, 0.1f);
  TEST_ASSERT_EQUAL_UINT16(number + 1, cold.state().trip.number);
  TEST_ASSERT_TRUE(cold.takeTripRecord(rec));
  TEST_ASSERT_EQUAL_UINT16(number, rec.number);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 5.0f, rec.km);
}

// Strom bleibt an: 5 min ohne Motor beendet die Fahrt; Stillstand über 10 s speichert einmal
void test_engine_off_and_save() {
  CarState s;
  support(s.link, {0x05, 0x0B, 0x0C, 0x0D});
  VehicleCalc calc;
  calc.load(simProfile(), nullptr);
  uint32_t now = 1;
  cruise(calc, s, now, 2.0f);
  calc.markSaved(now);
  TEST_ASSERT_FALSE(calc.saveDue(now));
  // Stand mit laufendem Motor
  for (int i = 0; i < 110; i++) {
    now += 100;
    s.speed.set(0, now);
    s.rpm.set(800, now);
    calc.step(s, now, 0.1f);
  }
  TEST_ASSERT_TRUE(calc.saveDue(now));  // über 10 s Stillstand
  calc.markSaved(now);
  for (int i = 0; i < 50; i++) {
    now += 100;
    calc.step(s, now, 0.1f);
  }
  TEST_ASSERT_FALSE(calc.saveDue(now));  // nur einmal je Stillstand
  // Motor aus, Werte bleiben aus (Zündung aus, Strom an)
  trip::TripRecord rec;
  for (int i = 0; i < 3100; i++) {  // 5 min 10 s
    now += 100;
    calc.step(s, now, 0.1f);  // Drehzahl veraltet -> Motor aus
  }
  TEST_ASSERT_TRUE(calc.takeTripRecord(rec));
  TEST_ASSERT_FALSE(calc.state().trip.active);
  TEST_ASSERT_TRUE(calc.saveDue(now));  // Fahrtende sofort sichern
  calc.markSaved(now);
  for (int i = 0; i < 1800; i++) {  // 3 min weiter ohne Motor: nichts ändert sich, nichts schreiben
    now += 100;
    calc.step(s, now, 0.1f);
    TEST_ASSERT_FALSE(calc.saveDue(now));
  }
  // Motor läuft wieder: neue Fahrt
  cruise(calc, s, now, 0.5f);
  TEST_ASSERT_TRUE(calc.state().trip.active);
  TEST_ASSERT_EQUAL_UINT16(rec.number + 1, calc.state().trip.number);
}

// Diesel ohne 0x5E: kein Verbrauch, also keine Schnitte (statt 0,0), die Fahrt zählt trotzdem km
void test_diesel_without_fuel_rate() {
  CarState s;
  support(s.link, {0x05, 0x0B, 0x0C, 0x0D, 0x10});
  VehicleCalc calc;
  Profile p = simProfile();
  p.fuel = FuelType::Diesel;
  calc.load(p, nullptr);
  uint32_t now = 1;
  cruise(calc, s, now, 3.0f);
  TEST_ASSERT_TRUE(calc.out().source == fuel::Source::None);
  TEST_ASSERT_FLOAT_IS_NAN(calc.out().avg1);
  TEST_ASSERT_FLOAT_IS_NAN(calc.out().avgTrip);
  TEST_ASSERT_FLOAT_IS_NAN(calc.out().instLph);
  TEST_ASSERT_TRUE(calc.state().trip.km > 1.0);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, static_cast<float>(calc.state().totalKm));  // Schnitte bleiben leer
}

// Schub: Momentanverbrauch exakt 0
void test_fuel_cut_zero() {
  CarState s;
  support(s.link, {0x03, 0x05, 0x0B, 0x0C, 0x0D});
  VehicleCalc calc;
  calc.load(simProfile(), nullptr);
  uint32_t now = 1;
  cruise(calc, s, now, 1.0f);
  now += 100;
  s.fuelSys.set(4, now);
  s.speed.set(60, now);
  s.rpm.set(1800, now);
  s.map.set(25, now);
  calc.step(s, now, 0.1f);
  TEST_ASSERT_TRUE(calc.out().fuelCut);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, calc.out().instLph);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, calc.out().instL100);
}

// Schub im Simulator (F7): ohne 0x03, mit Pedal (0x49) und Lambdasonde (0x15). Schaltpausen beim Beschleunigen
// (Gas weg, ausgekuppelt, Drehzahl fällt) dürfen nie Schub sein; Schub im Gang und Motorbremse im 2. Gang bei
// hoher Drehzahl müssen erkannt werden. Tempo kommt in ganzen km/h, die Lambdasonde nur alle 0,625 s.
static void cutScenario(bool learned) {
  DriveSim sim(11);
  CarState s;
  support(s.link, {0x05, 0x0B, 0x0C, 0x0D, 0x0F, 0x11, 0x15, 0x49});
  Profile p = simProfile();
  if (learned) {
    const float k[] = {7.3f, 13.2f, 19.5f, 26.0f, 32.0f};
    p.gearCount = 5;
    for (int i = 0; i < 5; i++) p.gears[i] = k[i];
  }
  VehicleCalc calc;
  calc.load(p, nullptr);
  const float dt = 0.125f;
  uint32_t now = 1;
  int shiftSteps = 0, shiftCut = 0, brakeSteps = 0, brakeCut = 0, overSteps = 0, overCut = 0;
  for (int i = 0; i < (int)(30 * 60 / dt); i++) {
    sim.step(dt);
    now += 125;
    const SimOutput& o = sim.out();
    s.speed.set(std::round(o.speedKmh), now);
    s.rpm.set(o.rpm, now);
    s.map.set(o.mapKpa, now);
    s.throttle.set(o.throttlePct, now);
    s.pedal.set(o.pedalPct, now);
    s.iat.set(o.iatC, now);
    s.coolant.set(o.coolantC, now);
    if (i % 5 == 0) s.o2V.set(o.o2V, now);
    calc.step(s, now, dt);
    if (o.coolantC < 45 || !o.engineOn) continue;  // kalt: Sonde fett, Gemisch offen
    const bool cut = calc.out().fuelCut;
    if (o.shifting && o.speedKmh > 16 && o.rpm > 1250) {
      shiftSteps++;
      shiftCut += cut;
    }
    if (sim.mode() == DriveSim::Mode::EngineBrake) {
      brakeSteps++;
      brakeCut += cut;
    }
    if (sim.mode() == DriveSim::Mode::Overrun && o.gear > 0 && o.speedKmh > 17) {
      overSteps++;
      overCut += cut;
    }
  }
  TEST_ASSERT_TRUE(shiftSteps > 50);
  TEST_ASSERT_EQUAL_INT(0, shiftCut);
  TEST_ASSERT_TRUE(brakeSteps > 100);
  TEST_ASSERT_TRUE(brakeCut > 0.75f * brakeSteps);  // Runterschalten vorher: kurz "ausgekuppelt", dann 0,4 s Verzug
  TEST_ASSERT_TRUE(overSteps > 100);
  TEST_ASSERT_TRUE(overCut > 0.7f * overSteps);    // die Simulation schaltet im Schub sprunghaft herunter
}

void test_cut_sim_learned_gears() { cutScenario(true); }
void test_cut_sim_without_gears() { cutScenario(false); }

// Automatische Tankerkennung mit 0x2F (A7): Anstieg um mindestens 8 % beim Motorstart = getankt
static void runLevel(VehicleCalc& calc, CarState& s, uint32_t& now, float rpm, float speed, float level, int steps) {
  for (int i = 0; i < steps; i++) {
    now += 100;
    s.speed.set(speed, now);
    s.rpm.set(rpm, now);
    s.map.set(45, now);
    s.iat.set(25, now);
    s.coolant.set(90, now);
    s.fuelLevel.set(level, now);
    calc.step(s, now, 0.1f);
  }
}

void test_refuel_detection() {
  CarState s;
  support(s.link, {0x05, 0x0B, 0x0C, 0x0D, 0x0F, 0x11, 0x2F});
  VehicleCalc calc;
  calc.load(simProfile(), nullptr);
  uint32_t now = 1;
  runLevel(calc, s, now, 2000, 50, 30, 600);  // 60 s Fahrt mit 30 % im Tank
  runLevel(calc, s, now, 0, 0, 30, 50);       // Motor aus (Tanken)
  const uint16_t before = calc.out().refuelSeq;
  runLevel(calc, s, now, 800, 0, 80, 120);    // Motor an, 12 s Stand mit 80 %
  TEST_ASSERT_EQUAL_UINT16(before + 1, calc.out().refuelSeq);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.5f * DriveSim::TANK_L, calc.out().refuelL);
  // Schwappen um 3 % beim nächsten Start zählt nicht
  runLevel(calc, s, now, 0, 0, 80, 50);
  runLevel(calc, s, now, 800, 0, 83, 120);
  TEST_ASSERT_EQUAL_UINT16(before + 1, calc.out().refuelSeq);
  // Ohne 0x2F gibt es keine Erkennung
  CarState s2;
  support(s2.link, {0x05, 0x0B, 0x0C, 0x0D, 0x0F, 0x11});
  VehicleCalc c2;
  c2.load(simProfile(), nullptr);
  runLevel(c2, s2, now, 2000, 50, 30, 100);
  runLevel(c2, s2, now, 0, 0, 30, 50);
  runLevel(c2, s2, now, 800, 0, 80, 120);
  TEST_ASSERT_EQUAL_UINT16(0, c2.out().refuelSeq);
}

// Spar-Ziel auto (A9): Ø der letzten 5 Füllungen 6,2 bzw. 4,6 -> 5,7 bzw. 4,3; nie unter 3,0
void test_auto_goal() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.7f, autoGoalFrom(6.2f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.3f, autoGoalFrom(4.6f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, autoGoalFrom(3.1f));
  TEST_ASSERT_FLOAT_IS_NAN(autoGoalFrom(NAN));
}

// Wartung: Tachostand eintragen, Zähler läuft mit, "Erledigt" setzt neu
void test_maintenance() {
  CarState s;
  support(s.link, {0x05, 0x0B, 0x0C, 0x0D, 0x0F, 0x11});
  VehicleCalc calc;
  calc.load(simProfile(), nullptr);
  uint32_t now = 1;
  calc.setOdo(100000);
  cruise(calc, s, now, 10);  // 10 km
  TEST_ASSERT_FLOAT_WITHIN(0.2f, 100010.0f, calc.out().odoKm);
  TEST_ASSERT_FLOAT_WITHIN(0.2f, cfg::OIL_INTERVAL_DEFAULT_KM - 10.0f, calc.out().oilLeftKm);
  calc.maintenanceDone(0);  // neu ab 100.010 km
  cruise(calc, s, now, 1);
  TEST_ASSERT_FLOAT_WITHIN(0.2f, cfg::OIL_INTERVAL_DEFAULT_KM - 1.0f, calc.out().oilLeftKm);
  calc.setInterval(0, 10000);  // gleicher letzter Termin, kürzeres Intervall
  cruise(calc, s, now, 0.1f);
  TEST_ASSERT_FLOAT_WITHIN(0.3f, 10000.0f - 1.1f, calc.out().oilLeftKm);
}

// Strecken-Faktor aus zwei Tachostand-Einträgen: Tacho 105 km, gezählt 100 km -> Faktor halb Richtung 1,05
void test_km_factor_from_odo() {
  CarState s;
  support(s.link, {0x05, 0x0B, 0x0C, 0x0D, 0x0F, 0x11});
  VehicleCalc calc;
  calc.load(simProfile(), nullptr);
  uint32_t now = 1;
  calc.setOdo(50000);
  cruise(calc, s, now, 100);
  calc.setOdo(50105);
  TEST_ASSERT_FLOAT_WITHIN(0.003f, 1.025f, calc.profile().kmFactor);  // gedämpft: (1,00 + 1,05) / 2
  cruise(calc, s, now, 0.01f);  // Ausgaben erst im nächsten Schritt
  TEST_ASSERT_FLOAT_WITHIN(0.2f, 50105.0f, calc.out().odoKm);
}

// Ohne 0x2F: nach einem kurzen Halt (Motor warm) und höchstens halbvollem Tank "Getankt?" fragen;
// bei kaltem Motor oder vollem Tank nicht
static void restart(VehicleCalc& calc, CarState& s, uint32_t& now, float coolant) {
  for (int i = 0; i < 50; i++) {  // 5 s Motor aus
    now += 100;
    s.speed.set(0, now);
    s.rpm.set(0, now);
    s.coolant.set(coolant, now);
    calc.step(s, now, 0.1f);
  }
  for (int i = 0; i < 30; i++) {  // 3 s Leerlauf
    now += 100;
    s.speed.set(0, now);
    s.rpm.set(800, now);
    s.coolant.set(coolant, now);
    calc.step(s, now, 0.1f);
  }
}

void test_refuel_ask_without_2f() {
  CarState s;
  support(s.link, {0x03, 0x05, 0x0B, 0x0C, 0x0D, 0x0F});
  VehicleCalc calc;
  Profile p = simProfile();
  p.tankL = 20;  // kleiner Tank: schnell halb leer (kurze Testlaufzeit)
  calc.load(p, nullptr);
  uint32_t now = 1;
  cruise(calc, s, now, 0.5f);
  TEST_ASSERT_EQUAL_UINT16(1, calc.out().refuelAskSeq);  // Tankinhalt unbekannt: beim Start fragen
  bool cal = false;
  calc.refuel(18.0f, 1.799f, true, trip::FillSource::Entered, cal);  // voll
  restart(calc, s, now, 85);
  TEST_ASSERT_EQUAL_UINT16(1, calc.out().refuelAskSeq);  // voll: keine Frage
  cruise(calc, s, now, 250.0f);  // gut die Hälfte verbraucht (ca. 5 l/100 km)
  restart(calc, s, now, 30);
  TEST_ASSERT_EQUAL_UINT16(1, calc.out().refuelAskSeq);  // kalt: langer Halt, keine Frage
  restart(calc, s, now, 85);
  TEST_ASSERT_EQUAL_UINT16(2, calc.out().refuelAskSeq);  // warm und halb leer: fragen
}

// Thermostat (A9): 16 min Fahrt, davon > 8 min über 50 km/h, Kühlmittel 65 °C -> einmal Hinweis;
// wird der Motor warm, verschwindet der Eintrag; beim nächsten Start (innerhalb von 10) kein neuer Hinweis
static void drive(VehicleCalc& calc, CarState& s, uint32_t& now, float minutes, float speed, float coolant) {
  for (int i = 0; i < static_cast<int>(minutes * 600); i++) {
    now += 100;
    s.speed.set(speed, now);
    s.rpm.set(2200, now);
    s.map.set(45, now);
    s.iat.set(15, now);
    s.fuelSys.set(2, now);
    s.coolant.set(coolant, now);
    calc.step(s, now, 0.1f);
  }
}

void test_thermostat() {
  CarState s;
  support(s.link, {0x05, 0x0B, 0x0C, 0x0D, 0x0F, 0x11});
  VehicleCalc calc;
  calc.load(simProfile(), nullptr);
  uint32_t now = 1;
  drive(calc, s, now, 7, 40, 65);
  TEST_ASSERT_EQUAL_UINT16(0, calc.out().thermoSeq);
  drive(calc, s, now, 9, 80, 65);  // 16 min, davon 9 über 50
  TEST_ASSERT_EQUAL_UINT16(1, calc.out().thermoSeq);
  TEST_ASSERT_TRUE(calc.out().thermoActive);
  drive(calc, s, now, 1, 80, 85);  // wird warm
  TEST_ASSERT_FALSE(calc.out().thermoActive);
  // Neustart: höchstens alle 10 Starts
  const PersistState saved = calc.state();
  VehicleCalc c2;
  c2.load(simProfile(), &saved);
  drive(c2, s, now, 17, 80, 65);
  TEST_ASSERT_EQUAL_UINT16(0, c2.out().thermoSeq);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_sim_drive_liters);
  RUN_TEST(test_refuel_and_calibration);
  RUN_TEST(test_trip_continues_after_restart);
  RUN_TEST(test_engine_off_and_save);
  RUN_TEST(test_fuel_cut_zero);
  RUN_TEST(test_cut_sim_learned_gears);
  RUN_TEST(test_cut_sim_without_gears);
  RUN_TEST(test_diesel_without_fuel_rate);
  RUN_TEST(test_refuel_detection);
  RUN_TEST(test_auto_goal);
  RUN_TEST(test_maintenance);
  RUN_TEST(test_thermostat);
  RUN_TEST(test_km_factor_from_odo);
  RUN_TEST(test_refuel_ask_without_2f);
  return UNITY_END();
}

#include "../board_runner.h"
