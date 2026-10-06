// Eco-Logik (A6 Gänge, A9 Hypermiling, A10 Leistung): Gang aus gelernten Übersetzungen,
// Schaltempfehlung, Eco-Score, Bremsenergie, Leistung und das Regelwerk der Spartipps.
// Reines C++ ohne Arduino und LVGL: Zeit und Messwerte kommen als Parameter, nativ testbar.
#pragma once
#include <cmath>
#include <cstdint>

#include "config.h"

namespace eco {

// ---------------------------------------------------------------------------
// Gänge (A6): k = km/h je 1000 U/min, Häufungen im Histogramm sind die Gänge
// ---------------------------------------------------------------------------

// Histogramm der k-Werte aus stabilen Phasen. POD, liegt in der Speicherdatei.
struct GearHistogram {
  uint16_t bins[cfg::GEAR_BINS];
  uint32_t total;

  void clear();
  void add(float k);
};

// k aus Tempo und Drehzahl; NAN ohne sinnvolle Werte
float ratioK(float speedKmh, float rpm);

// Häufungen suchen; schreibt höchstens max Werte aufsteigend nach out und liefert die Anzahl
int findGears(const GearHistogram& h, float* out, int max);

// Index des passenden Gangs (± 6 %), -1 = keiner
int matchGear(const float* gears, int count, float k);

// Anzeige-Nummer eines Gangs: Fehlt der 1. Gang (kleinster Wert > 10,5), zählt es ab 2
int gearNumber(const float* gears, int count, int index);

// Prüft, ob k über die letzten 1,5 s stabil war (A6 Lernen)
class StableK {
 public:
  // Neue Probe; liefert true, wenn k seit mindestens 1,5 s um weniger als 3 % schwankt
  bool add(float k, uint32_t nowMs);
  void reset() { count_ = 0; }

 private:
  static constexpr int N = 32;
  float k_[N] = {};
  uint32_t t_[N] = {};
  int head_ = 0;
  int count_ = 0;
};

// Angezeigter Gang
constexpr int8_t GEAR_NONE = -1;     // "–": Kupplung, Schalten, Motor aus, unbekannt
constexpr int8_t GEAR_NEUTRAL = 0;   // "N": ausgekuppelt rollen bzw. Stand

// Gang aus Tempo und Drehzahl. Ohne gelernte Gänge: N im Leerlauf, sonst GEAR_NONE.
// Liefert die Anzeige-Nummer (1 …) und in index den Index in gears (-1 ohne)
int8_t displayGear(const float* gears, int count, float speedKmh, float rpm, int& index);

// Schaltempfehlung (A9): Drehzahl ≥ shift_rpm, Pedal < 60 % und nicht geschlossen, kein Schub,
// nicht im höchsten Gang und im nächsten Gang noch mindestens nextMinRpm.
// Ohne gelernte Gänge (count 0) nur nach Drehzahl und Pedal; mit Gängen, aber ohne erkannten Gang nie.
bool shiftAdvice(float rpm, float pedalPct, bool pedalClosed, bool fuelCut, const float* gears, int count,
                 int index, float shiftRpm, float nextMinRpm);

// ---------------------------------------------------------------------------
// Fahrphysik (A9 Bremsenergie, A10 Leistung)
// ---------------------------------------------------------------------------

// Luft- und Rollwiderstand in N bei v m/s
float roadLoadN(float massKg, float cwA, float vMs);
// Leistung am Rad in W: (m·a + Luft + Rollen) · v
float wheelPowerW(float massKg, float cwA, float vMs, float accelMs2);
// Bremsleistung in W: max(0, −m·a − Luft − Rollen) · v
float brakePowerW(float massKg, float cwA, float vMs, float accelMs2);
// Energie in Liter Kraftstoff, den sie gekostet hat: J ÷ (0,25 · Heizwert)
float litersFromJoule(double joule, float heatMjPerL);

// ---------------------------------------------------------------------------
// Eco-Score (A9)
// ---------------------------------------------------------------------------
struct ScoreInput {
  float moveS = 0;        // Fahrzeit in Bewegung
  float rollS = 0;        // davon Schub oder Segeln ohne Verzögerung
  float pedalAbs = 0;     // Summe der Änderungen des geglätteten Pedals in %
  float shiftOpenS = 0;   // Zeit mit offener Schaltempfehlung
  float brakedL = 0;      // Gebremst in Liter
  float km = 0;
  float idleS = 0;        // Stand mit laufendem Motor
  float driveS = 0;       // Zeit mit laufendem Motor
};
struct ScoreParts {
  float roll, calm, early, brake, idle;
  float score;            // gewichtet, gerundet; NAN ohne Fahrzeit
};
ScoreParts score(const ScoreInput& in);

// ---------------------------------------------------------------------------
// Fahrzeug-Prüfung (A6): passt das Auto zu den gelernten Gängen?
// ---------------------------------------------------------------------------
class GearCheck {
 public:
  void reset();
  // Je Schritt mit stabiler Phase: matched = k passt zu einem gelernten Gang
  void add(bool matched, float dtS);
  // true, sobald nach ≥ 2 min stabiler Phasen höchstens 1/3 passt (einmal je Einschalten)
  bool mismatch() const;
  float stableS() const { return stableS_; }
  float matchedS() const { return matchedS_; }

 private:
  float stableS_ = 0;
  float matchedS_ = 0;
};

// ---------------------------------------------------------------------------
// Spartipps (A9 Regelwerk). Höchstens ein Tipp, 8 s sichtbar, Sperrzeiten; der Kalte-Motor-Hinweis
// steht, solange die Bedingung gilt, wenn kein Tipp aktiv ist.
// ---------------------------------------------------------------------------
enum class Tip : uint8_t { None, Coast, HardPedal, LateLift, Idle, Steady, Tempo, Cold, Thermo, COUNT };

struct TipInput {
  uint32_t nowMs = 0;
  bool engineOn = false;
  float speedKmh = NAN;
  float accelMs2 = NAN;     // gefiltert, + = schneller
  float pedalPct = NAN;
  bool fuelCut = false;
  int8_t gear = GEAR_NONE;  // GEAR_NEUTRAL = ausgekuppelt
  float coolantC = NAN;
  float rpm = NAN;
  float coldCoolantC = cfg::DEFAULT_COLD_COOLANT_C;
  float coldRpmLimit = cfg::DEFAULT_COLD_RPM_LIMIT;
  bool tipsEnabled = true;  // Menü (Etappe 7); der Kalte-Motor-Hinweis gilt immer
  bool quiet = false;       // Fenster offen oder vor < 3 s getippt: kein neuer Tipp
  float tempoSaveL = NAN;   // Spartempo: l/100 km bei 120 minus bei 100 (beide Klassen ≥ 5 km), sonst NAN
  // Mit MPU6050 (Etappe 8): kein "Sanfter Gas geben" beim Überholen oder am Berg
  float imuLongMs2 = NAN;
  float slopePct = NAN;
  uint16_t thermoSeq = 0;   // zählt hoch, wenn der Thermostat-Hinweis einmal erscheinen soll
};

class TipEngine {
 public:
  void reset();
  // Einmal je Snapshot; liefert den sichtbaren Hinweis (Tip::None = Feld leer)
  Tip update(const TipInput& in);
  Tip active() const { return active_; }
  // Standzeit für den Text "Stand 1:30"
  uint32_t standMs(uint32_t nowMs) const { return standSince_ ? nowMs - standSince_ : 0; }

 private:
  bool cause(Tip t, const TipInput& in) const;
  bool gone(Tip t, const TipInput& in) const;
  bool ready(Tip t, uint32_t nowMs) const;

  Tip active_ = Tip::None;
  uint32_t shownAt_ = 0;
  uint32_t lastShown_[static_cast<int>(Tip::COUNT)] = {};
  uint32_t lastAnyTip_ = 0;
  uint32_t coastSince_ = 0;
  uint32_t hardSince_ = 0;
  uint32_t pedalHighAt_ = 0;
  uint32_t standSince_ = 0;
  uint32_t fastSince_ = 0;  // über 115 km/h seit
  uint16_t seenThermo_ = 0;
  bool thermoInit_ = false;
  bool thermoPending_ = false;
  // Gleichmäßig: Proben der letzten 10 s (Tempo, Pedal)
  static constexpr int STEADY_N = 128;
  float stSpeed_[STEADY_N] = {};
  float stPedal_[STEADY_N] = {};
  uint32_t stT_[STEADY_N] = {};
  int stHead_ = 0;
  int stCount_ = 0;
  bool steadyNow_ = false;
};

}  // namespace eco
