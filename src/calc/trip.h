// Fahrt, Fahrtenbuch und Tankfüllungen (A8 Historie-Datensätze, A8 Fahrt-Ende ohne Uhr).
// Reines C++, nativ testbar. Die Datensätze sind POD und werden so in LittleFS gespeichert.
#pragma once
#include <cmath>
#include <cstdint>

namespace trip {

// Laufende Fahrt (in der Speicherdatei, überlebt Stromausfall)
struct TripState {
  uint8_t active;        // 1 = Fahrt läuft
  uint16_t number;       // fortlaufende Nummer
  // Summen als double: viele kleine Schritte würden in float merklich wegrunden
  double km;
  double liters;
  double durationS;      // Zeit mit laufendem Motor
  double idleS;          // Stand mit laufendem Motor
  double idleL;
  double cutS;           // Schubabschaltung
  double cost;           // Euro mit dem jeweiligen Mischpreis, beim Speichern festgeschrieben
  float maxRpm;
  float maxCoolantC;
  uint8_t costKnown;     // 0 = noch nie ein Preis eingegeben
};

void start(TripState& t, uint16_t number);

// Fahrtdatensatz im Fahrtenbuch (A8: ca. 64 Byte). Gebremst und Eco-Score kommen mit Etappe 4.
struct TripRecord {
  uint16_t number;
  uint8_t profileId;
  uint8_t reserved;
  uint32_t date;         // mit GPS: JJJJMMTT, sonst 0 (Etappe 8)
  float km;
  float liters;
  float durationS;
  float idleS;
  float cutS;
  float brakedL;         // NAN bis Etappe 4
  float ecoScore;        // NAN bis Etappe 4
  float maxRpm;
  float maxCoolantC;
  float cost;            // NAN ohne Preis
  float spare[4];
};
static_assert(sizeof(TripRecord) == 64, "Fahrtdatensatz soll 64 Byte haben");

TripRecord toRecord(const TripState& t, uint8_t profileId);

// Liegt eine kurze Pause vor, läuft die Fahrt beim nächsten Start weiter (A8, ohne GPS):
// Motor beim Abstellen warm (≥ 70 °C) und beim Start höchstens 4 °C kälter. Fehlt ein Wert: neue Fahrt.
bool continues(float coolantAtStopC, float coolantAtStartC);

// Tankfüllung (A8): Nummer, Datum, km seit der letzten Füllung, Liter, Preis, Ø, Kosten je 100 km
enum class FillSource : uint8_t { Detected, Computed, Entered };
struct FillRecord {
  uint16_t number;
  uint8_t profileId;
  uint8_t full;          // 1 = vollgetankt
  uint32_t date;         // mit GPS, sonst 0
  float kmSinceLast;
  float liters;
  float price;           // bezahlter Preis €/l inkl. ⁹
  float l100;            // Ø seit der vorigen Füllung (berechnete Liter)
  float costPer100;      // Ø · Mischpreis dieser Strecke
  FillSource source;
  uint8_t reserved[3];
};

}  // namespace trip
