// Verbrauch, Kalibrierung, Tankmodell, Mischpreis, Preis-Eingabe und Reichweite (A7).
// Reines C++ ohne Arduino und LVGL: alle Werte kommen als Parameter herein, fehlende als NAN.
#pragma once
#include <cmath>
#include <cstdint>

#include "core/profile.h"

namespace fuel {

// Woraus der Verbrauch gerechnet wird; die erste verfügbare Quelle gewinnt (A7)
// AbsLoad (7.10.2026): Luft je Hub aus der absoluten Last (0x43), vom Steuergerät selbst gerechnet; braucht
// keinen geschätzten Füllgrad wie Speed-Density.
enum class Source : uint8_t { None, FuelRate, Maf, SpeedDensity, AbsLoad };

// Quelle aus den unterstützten PIDs. Diesel nur über 0x5E: Luftmasse allein reicht beim
// mager laufenden Diesel nicht (A7). ANNAHME: Der Weg über Breitband-Lambda (0x34–0x3B) fehlt
// in dieser Version, Diesel ohne 0x5E zeigen "nicht verfügbar".
Source chooseSource(FuelType fuel, bool hasFuelRate, bool hasMaf, bool hasMap, bool hasRpm, bool hasAbsLoad = false);

// Luft g/s aus der absoluten Last: Last/100 · 1,184 g/l · Hubraum · Drehzahl/120 (SAE J1979)
float absLoadAirGs(float absLoadPct, float displacementL, float rpm);

// Rohwerte eines Augenblicks; NAN = nicht unterstützt oder veraltet
struct Input {
  float speedKmh = NAN;
  float rpm = NAN;
  float mapKpa = NAN;
  float iatC = NAN;
  float stftPct = NAN;
  float ltftPct = NAN;
  float mafGs = NAN;
  float absLoadPct = NAN;  // 0x43
  float fuelRateLph = NAN;
  float lambda = NAN;       // Soll-Lambda (0x44)
  float fuelSys = NAN;      // 0x03
  float throttlePct = NAN;  // 0x11
  float pedalPct = NAN;     // 0x49 (Gaspedal)
  float pedalClosedPct = NAN;  // kleinster Pedalwert seit dem Laden (Pedal losgelassen)
};

struct Engine {
  FuelType fuel = FuelType::Petrol;
  float displacementL = 0;
  float ve = 0;
  float fuelCal = 1.0f;
};

float densityGPerL(FuelType fuel);

// Speed-Density: angesaugte Luft in g/s (ideales Gasgesetz, Viertakter, A7)
float speedDensityAirGs(float mapKpa, float displacementL, float rpm, float ve, float iatC);

// Verbrauch in l/h aus der gewählten Quelle, mit Gemischkorrektur, λ und fuel_cal.
// NAN, wenn ein nötiger Wert fehlt. Schubabschaltung prüft der Aufrufer (isFuelCut).
float rateLph(Source src, const Engine& e, const Input& in);

// Schubabschaltung (A7): 0x03 = 4, sonst Gaspedal losgelassen (ersatzweise Drosselklappe ≈ zu) bei
// > 1200 U/min und > 15 km/h. Nach der ersten Fahrt (7.10.2026) gilt die Ersatzregel auch, wenn das Auto
// 0x03 kennt: Viele Steuergeräte melden im Schub nie die 4, die Anzeige erkannte dann keinen Schub.
// throttleClosedPct = kleinster Drosselklappenwert seit dem Verbinden (NAN = unbekannt).
bool isFuelCut(bool hasFuelSys, const Input& in, float throttleClosedPct);

// l/100 km, erst ab 5 km/h (A7); darunter NAN (die Anzeige zeigt dann l/h)
float litersPer100(float lph, float speedKmh);

// Selbstkalibrierung zwischen zwei Vollbetankungen (A7). applied = false, wenn die Füllung
// nicht zählt (unter 150 km oder Verhältnis außerhalb 0,7–1,3); dann bleibt fuel_cal gleich.
float calibrate(float fuelCal, float filledL, float computedL, float km, bool& applied);

// Tankmodell nach dem Tanken (A7): voll = Tankgröße, sonst Rest + getankt (höchstens Tankgröße)
float tankAfterRefuel(float restL, float addedL, bool full, float tankL);

// Mischpreis im Tank (A7): (Rest · alter Mischpreis + getankt · Preis) ÷ (Rest + getankt).
// Ohne alten Mischpreis oder Rest gilt der neue Preis.
float mixPrice(float restL, float oldMix, float addedL, float price);

// Preis-Eingabe in Euro und Cent; die ⁹ (9/10 Cent) kommt immer dazu (A7): 179 -> 1,799 €/l
float priceFromCents(int cents);
// Vorbelegung des Tank-Fensters aus dem letzten Preis: 1,699 -> 169
int centsFromPrice(float price);

// Prognose-Verbrauch (A7): 50 % Ø 100 km + 30 % Ø 10 km + 20 % Ø der letzten Tankfüllungen.
// Fehlende Werte (NAN) geben ihr Gewicht anteilig an die vorhandenen ab. Alle fehlen: NAN.
float prognosis(float avg100, float avg10, float avgFills);

// Reichweite in km = Rest-Liter ÷ Verbrauch · 100; NAN ohne Rest oder Verbrauch
float rangeKm(float restL, float l100);

}  // namespace fuel
