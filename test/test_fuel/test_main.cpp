// Verbrauch, Kalibrierung, Tankmodell, Mischpreis, Preis-Eingabe, Reichweite (M Prüfwerte, A7)
#include "calc/fuel.h"
#include "calc/fuel_cut.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

static fuel::Engine modus() {
  fuel::Engine e;
  e.fuel = FuelType::Petrol;
  e.displacementL = 1.149f;
  e.ve = 0.85f;
  e.fuelCal = 1.0f;
  return e;
}

// 32 kPa, 1,149 l, 780 U/min, VE 0,85, 25 °C, AFR 14,7, Trims 0, fuel_cal 1 -> ≈ 0,78 l/h (± 0,02)
void test_speed_density() {
  fuel::Input in;
  in.mapKpa = 32;
  in.rpm = 780;
  in.iatC = 25;
  in.stftPct = 0;
  in.ltftPct = 0;
  const float lph = fuel::rateLph(fuel::Source::SpeedDensity, modus(), in);
  TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.78f, lph);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 2.37f, fuel::speedDensityAirGs(32, 1.149f, 780, 0.85f, 25));
  // Gemischkorrektur +10 % und λ 0,9 (Anfettung) erhöhen den Verbrauch
  in.stftPct = 6;
  in.ltftPct = 4;
  TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.78f * 1.1f, fuel::rateLph(fuel::Source::SpeedDensity, modus(), in));
  in.stftPct = in.ltftPct = 0;
  in.lambda = 0.9f;
  TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.78f / 0.9f, fuel::rateLph(fuel::Source::SpeedDensity, modus(), in));
  // ohne Saugrohrdruck kein Wert
  in.mapKpa = NAN;
  TEST_ASSERT_FLOAT_IS_NAN(fuel::rateLph(fuel::Source::SpeedDensity, modus(), in));
}

void test_source_order() {
  using fuel::Source;
  TEST_ASSERT_TRUE(fuel::chooseSource(FuelType::Petrol, true, true, true, true) == Source::FuelRate);
  TEST_ASSERT_TRUE(fuel::chooseSource(FuelType::Petrol, false, true, true, true) == Source::Maf);
  TEST_ASSERT_TRUE(fuel::chooseSource(FuelType::Petrol, false, false, true, true) == Source::SpeedDensity);
  TEST_ASSERT_TRUE(fuel::chooseSource(FuelType::Petrol, false, false, true, true, true) == Source::AbsLoad);
  TEST_ASSERT_TRUE(fuel::chooseSource(FuelType::Petrol, false, true, true, true, true) == Source::Maf);
  // 1,4 l, 2000 U/min, absolute Last 50 %: 0,5 · 1,184 · 1,4 · 2000/120 = 13,8 g/s Luft
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 13.81f, fuel::absLoadAirGs(50, 1.4f, 2000));
  TEST_ASSERT_TRUE(fuel::chooseSource(FuelType::Petrol, false, false, false, true) == Source::None);
  TEST_ASSERT_TRUE(fuel::chooseSource(FuelType::Diesel, false, true, true, true) == Source::None);
  TEST_ASSERT_TRUE(fuel::chooseSource(FuelType::Diesel, true, true, true, true) == Source::FuelRate);
  TEST_ASSERT_TRUE(fuel::chooseSource(FuelType::Diesel, false, true, true, true, false, true) == Source::Maf);  // mit λ
}

// Status 0x03 = 4 -> Schub; ohne 0x03 über Drosselklappe, Drehzahl und Tempo
void test_fuel_cut() {
  fuel::Input in;
  in.fuelSys = 4;
  TEST_ASSERT_TRUE(fuel::isFuelCut(true, in, NAN));
  in.fuelSys = 2;
  TEST_ASSERT_FALSE(fuel::isFuelCut(true, in, NAN));
  // 0x03 bekannt, meldet aber nie 4: Pedal losgelassen bei 1800 U/min und 60 km/h = Schub
  in.pedalPct = 14.5f;
  in.pedalClosedPct = 14.1f;
  in.rpm = 1800;
  in.speedKmh = 60;
  TEST_ASSERT_TRUE(fuel::isFuelCut(true, in, NAN));
  in.pedalPct = 25;
  TEST_ASSERT_FALSE(fuel::isFuelCut(true, in, NAN));  // Gas
  in.pedalPct = NAN;
  in.pedalClosedPct = NAN;
  in.throttlePct = 12.5f;
  in.rpm = 1800;
  in.speedKmh = 60;
  TEST_ASSERT_TRUE(fuel::isFuelCut(false, in, 11.8f));   // Klappe zu (12,5 ≤ 11,8 + 1,5)
  in.rpm = 1100;
  TEST_ASSERT_FALSE(fuel::isFuelCut(false, in, 11.8f));  // zu niedrige Drehzahl
  in.rpm = 1800;
  in.throttlePct = 20;
  TEST_ASSERT_FALSE(fuel::isFuelCut(false, in, 11.8f));  // Gas
  // hohe Drehzahl im kleinen Gang: Klappe 17 % (öffnet im Schub etwas), Toleranz 1,5 + 2 · 2,8 = 7,1
  in.rpm = 4000;
  in.throttlePct = 17;
  TEST_ASSERT_TRUE(fuel::isFuelCut(false, in, 11.8f));
  in.o2V = 0.7f;  // Sonde fett: doch kein Schub
  TEST_ASSERT_FALSE(fuel::isFuelCut(false, in, 11.8f));
  in.o2V = 0.05f;  // Sonde mager: Schub bestätigt
  TEST_ASSERT_TRUE(fuel::isFuelCut(false, in, 11.8f));
  in.throttlePct = NAN;  // nur die Sonde
  TEST_ASSERT_TRUE(fuel::isFuelCut(false, in, 11.8f));
  in.o2V = NAN;
}

// Schub genauer (F7): Einschaltverzug 0,4 s, aus sofort beim Gas, Gang ± 5 %, Lambda nur frisch,
// keine Obergrenze für die Drehzahl
static const float GEARS[] = {7.3f, 13.2f, 19.5f, 26.0f, 32.0f};

static fuel::CutDetector::In cutIn(float speed, float rpm, float pedal, uint32_t rpmT) {
  fuel::CutDetector::In x;
  x.in.speedKmh = speed;
  x.in.rpm = rpm;
  x.in.pedalPct = pedal;
  x.in.pedalClosedPct = 0;
  x.rpmT = rpmT;
  x.gears = GEARS;
  x.gearCount = 5;
  return x;
}

void test_cut_detector() {
  fuel::CutDetector d;
  // 60 km/h im 3. Gang (k 19,5 -> 3077 U/min), Fuß weg: erst nach 0,4 s Schub
  uint32_t t = 1000;
  for (; t < 1300; t += 100) TEST_ASSERT_FALSE(d.step(cutIn(60, 3077, 0, t), t));
  TEST_ASSERT_TRUE(d.watch());
  TEST_ASSERT_TRUE(d.why() == fuel::CutDetector::Why::Waiting);
  t += 100;
  TEST_ASSERT_TRUE(d.step(cutIn(60, 3077, 0, t), t));
  char buf[64];
  d.describe(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("ja: Pedal 0 %, Gang 3, ohne Lambda", buf);
  // Gas: sofort aus
  t += 100;
  TEST_ASSERT_FALSE(d.step(cutIn(60, 3077, 8, t), t));
  TEST_ASSERT_TRUE(d.why() == fuel::CutDetector::Why::Gas);
  // 0x03 = 4, aber Gas: kein Schub
  fuel::CutDetector::In g = cutIn(60, 3077, 8, t);
  g.hasFuelSys = true;
  g.in.fuelSys = 4;
  TEST_ASSERT_FALSE(d.step(g, t));

  // Frische Lambda-Messung fett: kein Schub; ist sie älter als 0,5 s, entscheiden Pedal und Gang allein
  d.reset();
  t = 5000;
  fuel::CutDetector::In x = cutIn(60, 3077, 0, t);
  x.o2V = 0.7f;
  x.o2AgeMs = 200;
  for (int i = 0; i < 10; i++, t += 100) {
    x.rpmT = t;
    TEST_ASSERT_FALSE(d.step(x, t));
  }
  TEST_ASSERT_TRUE(d.why() == fuel::CutDetector::Why::Rich);
  x.o2AgeMs = 800;
  for (int i = 0; i < 4; i++, t += 100) {
    x.rpmT = t;
    d.step(x, t);
  }
  x.rpmT = t;
  TEST_ASSERT_TRUE(d.step(x, t));
  // frisch und mager: bestätigt
  x.o2V = 0.05f;
  x.o2AgeMs = 100;
  t += 100;
  x.rpmT = t;
  TEST_ASSERT_TRUE(d.step(x, t));
  d.describe(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("ja: Pedal 0 %, Gang 3, Lambda 0,05 V 0,1 s", buf);

  // Passt zu keinem Gang (zwischen 3. und 4.): kein Schub
  d.reset();
  t = 9000;
  for (int i = 0; i < 8; i++, t += 100) TEST_ASSERT_FALSE(d.step(cutIn(60, 2650, 0, t), t));
  TEST_ASSERT_TRUE(d.why() == fuel::CutDetector::Why::NoGear);

  // Hohe Drehzahl mit Motorbremse: 80 km/h im 2. Gang = 6060 U/min, keine Obergrenze
  d.reset();
  t = 12000;
  bool on = false;
  for (int i = 0; i < 6; i++, t += 100) on = d.step(cutIn(80, 6061, 0, t), t);
  TEST_ASSERT_TRUE(on);

  // Ohne gelernte Gänge: ausgekuppelt (Drehzahl fällt 1800 U/min je s bei gleichem Tempo) = kein Schub
  d.reset();
  t = 15000;
  float rpm = 2900;
  for (int i = 0; i < 8; i++, t += 100, rpm -= 180) {
    fuel::CutDetector::In y = cutIn(40, rpm, 0, t);
    y.gearCount = 0;
    TEST_ASSERT_FALSE(d.step(y, t));
  }
  TEST_ASSERT_TRUE(d.why() == fuel::CutDetector::Why::Declutched);
}

// 4 km/h -> kein l/100-Wert (Anzeige l/h)
void test_l100_min_speed() {
  TEST_ASSERT_FLOAT_IS_NAN(fuel::litersPer100(1.0f, 4.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, fuel::litersPer100(2.5f, 50.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, fuel::litersPer100(0.0f, 50.0f));  // Schub
}

// getankt 40 l, berechnet 36,4 l, 400 km -> fuel_cal · √(40/36,4) = · 1,0483
void test_calibration() {
  bool applied = false;
  const float cal = fuel::calibrate(1.0f, 40.0f, 36.4f, 400.0f, applied);
  TEST_ASSERT_TRUE(applied);
  TEST_ASSERT_FLOAT_WITHIN(0.0005f, 1.0483f, cal);
  TEST_ASSERT_FLOAT_WITHIN(0.0005f, 0.95f * 1.0483f, fuel::calibrate(0.95f, 40.0f, 36.4f, 400.0f, applied));
}

// Verhältnis 1,4 oder < 150 km -> keine Änderung
void test_calibration_invalid() {
  bool applied = true;
  TEST_ASSERT_EQUAL_FLOAT(1.0f, fuel::calibrate(1.0f, 42.0f, 30.0f, 400.0f, applied));  // 1,4
  TEST_ASSERT_FALSE(applied);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, fuel::calibrate(1.0f, 40.0f, 36.4f, 149.0f, applied));
  TEST_ASSERT_FALSE(applied);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, fuel::calibrate(1.0f, 20.0f, 30.0f, 400.0f, applied));  // 0,67
  TEST_ASSERT_FALSE(applied);
  // Grenzen 0,7–1,3 für fuel_cal
  TEST_ASSERT_EQUAL_FLOAT(1.3f, fuel::calibrate(1.25f, 38.9f, 30.0f, 400.0f, applied));
}

// Rest 28,2 l, 10 l getankt, nicht voll, Tank 49 l -> 38,2 l; vollgetankt: 49 l
void test_tank_model() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 38.2f, fuel::tankAfterRefuel(28.2f, 10.0f, false, 49.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 49.0f, fuel::tankAfterRefuel(28.2f, 10.0f, true, 49.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 49.0f, fuel::tankAfterRefuel(45.0f, 10.0f, false, 49.0f));  // höchstens Tankgröße
}

// 15 l Rest zu 1,799 € + 30 l zu 1,699 € -> 1,7323 €/l
void test_mix_price() {
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.7323f, fuel::mixPrice(15.0f, 1.799f, 30.0f, 1.699f));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.699f, fuel::mixPrice(15.0f, NAN, 30.0f, 1.699f));  // erste Füllung
}

// "1,79" -> 1,799 €/l; Vorbelegung aus 1,699 ergibt "1,69"
void test_price_input() {
  TEST_ASSERT_FLOAT_WITHIN(0.00001f, 1.799f, fuel::priceFromCents(179));
  TEST_ASSERT_EQUAL_INT(169, fuel::centsFromPrice(1.699f));
  TEST_ASSERT_EQUAL_INT(179, fuel::centsFromPrice(fuel::priceFromCents(179)));
}

// 28 l Rest, Ø100 6,0, Ø10 7,0, Tankfüllungen 6,2 -> Prognose 6,34 -> 441 km
void test_range() {
  const float p = fuel::prognosis(6.0f, 7.0f, 6.2f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.34f, p);
  TEST_ASSERT_EQUAL_INT(441, (int)fuel::rangeKm(28.0f, p));
  // fehlende Daten: Gewichte verteilt (nur Ø100 und Ø10: 5/8 und 3/8)
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.375f, fuel::prognosis(6.0f, 7.0f, NAN));
  TEST_ASSERT_FLOAT_IS_NAN(fuel::prognosis(NAN, NAN, NAN));
  TEST_ASSERT_FLOAT_IS_NAN(fuel::rangeKm(NAN, 6.0f));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_speed_density);
  RUN_TEST(test_source_order);
  RUN_TEST(test_fuel_cut);
  RUN_TEST(test_cut_detector);
  RUN_TEST(test_l100_min_speed);
  RUN_TEST(test_calibration);
  RUN_TEST(test_calibration_invalid);
  RUN_TEST(test_tank_model);
  RUN_TEST(test_mix_price);
  RUN_TEST(test_price_input);
  RUN_TEST(test_range);
  return UNITY_END();
}

#include "../board_runner.h"
