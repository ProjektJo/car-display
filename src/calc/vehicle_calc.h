// Rechnung für das geladene Fahrzeugprofil (calcTask, A5/A7/A8): Momentanverbrauch,
// Strecken-Mittelwerte, Tank, Reichweite, laufende Fahrt, Tanken und Kalibrierung.
// Reines C++: Zeit und Messwerte kommen als Parameter, nativ testbar (auch mit dem Simulator).
#pragma once
#include <cmath>
#include <cstdint>

#include "calc/fuel.h"
#include "calc/persist.h"
#include "calc/trip.h"
#include "core/car_state.h"
#include "core/profile.h"

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
  };

  // Profil laden. saved = gespeicherte Summen dieses Profils oder nullptr (neues Profil).
  // nowMs: ab hier läuft die Minute bis zum nächsten regulären Speichern
  void load(const Profile& p, const PersistState* saved, uint32_t nowMs = 0);
  void unload() { active_ = false; }
  bool active() const { return active_; }
  const Profile& profile() const { return profile_; }
  const PersistState& state() const { return st_; }
  const Outputs& out() const { return out_; }

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

 private:
  void decideTrip(const CarState& s, uint32_t nowMs);
  void finishTrip();
  void updateOutputs(const CarState& s, uint32_t nowMs, float dtS);

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

  // 1-s-Fenster für den Momentanverbrauch: je Schritt Dauer, Liter·s und km/h·s
  static constexpr int WIN = 16;
  float winDt_[WIN] = {};
  float winLph_[WIN] = {};
  float winSpeed_[WIN] = {};
  int winHead_ = 0;
};
