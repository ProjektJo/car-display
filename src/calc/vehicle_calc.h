// Rechnung für das geladene Fahrzeugprofil (calcTask, A5/A7/A8): Momentanverbrauch,
// Strecken-Mittelwerte, Tank, Reichweite, laufende Fahrt, Tanken und Kalibrierung.
// Reines C++: Zeit und Messwerte kommen als Parameter, nativ testbar (auch mit dem Simulator).
#pragma once
#include <cmath>
#include <cstdint>

#include "calc/eco.h"
#include "calc/fuel.h"
#include "calc/perf.h"
#include "calc/persist.h"
#include "calc/trip.h"
#include "core/car_state.h"
#include "core/profile.h"

// Spar-Ziel "auto" aus einem Schnitt (A9, Z 13): minus 0,5 l über 5 l, sonst minus 0,3 l; mindestens 3,0
float autoGoalFrom(float avgL100);

class VehicleCalc {
 public:
  struct Outputs {
    float instLph = NAN;       // 1-s-Mittel
    float instL100 = NAN;      // ab 5 km/h, sonst NAN (Anzeige dann l/h)
    bool fuelCut = false;
    float avg1 = NAN, avg10 = NAN, avg100 = NAN;
    float avgTank = NAN, avgTrip = NAN, avgProfile = NAN, avgFills = NAN;
    float tankL = NAN;
    float prognosis = NAN;     // geglättet (τ 60 s)
    float rangeKm = NAN;
    float tripKm = NAN, tripL = NAN, tripCost = NAN, tripDurationS = NAN;
    float mixPrice = NAN, pumpPrice = NAN;
    fuel::Source source = fuel::Source::None;
    // Eco (A6, A9)
    int8_t gear = eco::GEAR_NONE;  // Anzeige: Nummer, GEAR_NEUTRAL = "N", GEAR_NONE = "–"
    bool shiftAdvice = false;      // Hochschalten empfohlen (der Pfeil kommt nach 1 s, UI)
    float accelMs2 = NAN;          // Beschleunigung aus dem Tempo, gefiltert
    float pedalPct = NAN;          // Gaspedal (0x49), sonst Drosselklappe (0x11)
    float ecoScore = NAN;          // laufende Fahrt
    float cutSavedL = NAN;         // Schub gespart in dieser Fahrt
    float brakedL = NAN;           // Gebremst in dieser Fahrt
    bool gearMismatch = false;     // Fahrzeug-Prüfung: Gänge passen nicht zum Profil
    // Tankfüllung und Fahrt (Fahrt & Tank, Reichweite)
    float fillKm = NAN;            // gefahren seit dem Tanken
    float fillL = NAN;             // verbraucht seit dem Tanken (berechnet)
    float tripIdleS = NAN;
    float sinceFullL = NAN;        // berechnete Liter seit der letzten Vollbetankung (Vorschlag "Getankt")
    // Automatische Tankerkennung (A7): zählt hoch, wenn ein Tankvorgang erkannt wurde
    uint16_t refuelSeq = 0;
    float refuelL = NAN;
    // Sport und Sprint (A10)
    float powerKw = NAN;           // geschätzte Leistung am Rad
    float tripVmax = NAN, tripKwPeak = NAN;
    // Spar-Ziel und Wartung (Etappe 7)
    float goalL100 = NAN;          // aktuelles Ziel, NAN = aus
    float goalBase = NAN;          // bei auto: Schnitt, aus dem das Ziel stammt ("6,2 → 5,7")
    float odoKm = NAN;             // Tachostand, NAN = nie eingetragen
    float oilLeftKm = NAN, inspLeftKm = NAN;
    // Thermostat-Check (A9)
    uint16_t thermoSeq = 0;
    bool thermoActive = false;
  };

  // Profil laden. saved = gespeicherte Summen dieses Profils oder nullptr (neues Profil).
  // nowMs: ab hier läuft die Minute bis zum nächsten regulären Speichern
  void load(const Profile& p, const PersistState* saved, uint32_t nowMs = 0);
  void unload() { active_ = false; }
  bool active() const { return active_; }
  const Profile& profile() const { return profile_; }
  const PersistState& state() const { return st_; }
  const Outputs& out() const { return out_; }
  const perf::SprintMeter& sprint() const { return sprint_; }

  // Ein Rechenschritt mit den aktuellen Werten (s.now ist nicht nötig, nowMs zählt)
  void step(const CarState& s, uint32_t nowMs, float dtS);

  // Ereignisse für calcTask
  bool takeTripRecord(trip::TripRecord& r);   // eine beendete Fahrt fürs Fahrtenbuch
  bool takeProfileChanged();                  // fuel_cal (später Gänge) geändert: Profil speichern
  bool saveDue(uint32_t nowMs) const;
  void markSaved(uint32_t nowMs);

  // Tanken (Tank-Fenster ab Etappe 5). Rechnet Tankmodell, Mischpreis und Kalibrierung.
  trip::FillRecord refuel(float liters, float price, bool full, trip::FillSource src, bool& calApplied);
  // Fahrt von Hand beenden (Menü, Etappe 7); die nächste beginnt mit dem nächsten Motorlauf
  void endTrip();
  // Fahrzeug-Prüfung mit "Ja" beantwortet: Profil bleibt, gelernte Gänge verwerfen und neu lernen (A6)
  void relearnGears();
  // Fahrzeug-Prüfung ohne Antwort geschlossen bzw. anderes Fahrzeug gewählt: nicht mehr fragen
  void dismissGearCheck();

  // Einstellungen aus dem Menü (Etappe 7)
  void setGoal(uint8_t mode, float fixL100);   // 0 aus, 1 auto, 2 fest
  void setBody(uint8_t body);                  // Fahrzeugart (Profil)
  void setColdRpm(uint16_t rpm);               // Kalt-Grenze (Profil)
  void setOdo(float km);                       // Tachostand einmal eintragen
  void maintenanceDone(int which);             // 0 Ölwechsel, 1 Inspektion: Zähler neu
  void setInterval(int which, float km);
  void resetAverages(uint8_t mask);            // Bit 0: 1 km, 1: 10 km, 2: 100 km, 3: Tank
  void setKmFactor(float f);                   // aus dem GPS-Vergleich (A7)
  void setVehicle(FuelType fuel, float displacementL, float tankL, uint16_t powerKw);  // Menü "Fahrzeug"

 private:
  void decideTrip(const CarState& s, uint32_t nowMs);
  void finishTrip();
  void updateOutputs(const CarState& s, uint32_t nowMs, float dtS);
  void stepEco(const CarState& s, uint32_t nowMs, float dtS, bool engineOn, bool cut, float lph, float dkm, float dl);
  void updateGears(uint32_t nowMs);

  bool active_ = false;
  Profile profile_;
  PersistState st_;
  Outputs out_;

  bool tripDecided_ = false;
  float coolantAtStopC_ = NAN;
  bool hasTripRecord_ = false;
  trip::TripRecord tripRecord_ = {};
  bool profileChanged_ = false;

  float throttleClosed_ = NAN;   // kleinster Drosselklappenwert seit dem Laden
  uint32_t engineOffSinceMs_ = 0;
  uint32_t standstillSinceMs_ = 0;
  bool standstillSaved_ = false;
  bool saveNow_ = false;
  uint32_t lastSaveMs_ = 0;
  double kmAtSave_ = 0, litersAtSave_ = 0;  // Stand beim letzten Speichern
  float levelSmooth_ = NAN;      // geglätteter Füllstand (0x2F)

  // Eco
  float pedalClosed_ = NAN;      // kleinster Pedalwert seit dem Laden
  float pedalMax_ = NAN;         // größter Pedalwert seit dem Laden (Sprint: Vollgas)
  float pedalSmooth_ = NAN;      // geglättetes Pedal für "ruhiges Gas"
  uint32_t speedT_ = 0;          // Zeitstempel der letzten Tempo-Messung
  float speedPrev_ = NAN;
  float accel_ = NAN;
  eco::StableK stable_;
  eco::GearCheck check_;
  bool checkAsked_ = false;      // höchstens einmal je Einschalten (bleibt über load() hinweg)
  bool learnPaused_ = false;     // Prüfung offen: nichts lernen, bis die Antwort da ist

  // Tankerkennung
  void stepRefuelDetect(const CarState& s, uint32_t nowMs, bool engineOn);
  bool engineWasOn_ = false;
  uint32_t detectSince_ = 0;     // Motorstart: Füllstand wird gemittelt
  bool detectDone_ = true;       // erst nach dem ersten Motorstart prüfen
  double detectSum_ = 0;
  uint32_t detectN_ = 0;
  uint32_t lastGearSearchMs_ = 0;
  perf::SprintMeter sprint_;
  // Etappe 8
  void stepThermo(const CarState& s, uint32_t nowMs, float dtS, bool engineOn);
  uint32_t gpsDate_ = 0;
  uint32_t gpsEpochAtStop_ = 0;  // GPS-Zeit beim letzten Speichern vor diesem Start (Fahrtende, A8)
  float thermoDriveS_ = 0, thermoFastS_ = 0, iatStart_ = NAN;
  bool thermoFired_ = false;
  uint8_t goalMode_ = 0;
  float goalFix_ = cfg::GOAL_FIX_DEFAULT;
  // Spartempo: ruhige Konstantfahrt seit
  uint32_t tempoSince_ = 0;
  float tempoRefV_ = NAN, tempoRefP_ = NAN;

  // 1-s-Fenster für den Momentanverbrauch: je Schritt Dauer, Liter·s und km/h·s
  static constexpr int WIN = 16;
  float winDt_[WIN] = {};
  float winLph_[WIN] = {};
  float winSpeed_[WIN] = {};
  int winHead_ = 0;
};
