// Schuberkennung mit Gang, Kupplung und Lambda-Alter (Etappe 1b, F7).
// Reines C++ ohne Arduino: Zeit und Messwerte kommen als Parameter, nativ testbar.
//
// Schub, wenn
// - 0x03 = 4 meldet (das Steuergerät weiß es selbst), oder
// - der Fuß vom Gas ist (Pedal, ersatzweise Drosselklappe), über 1200 U/min und 15 km/h,
//   Tempo/Drehzahl zu einem gelernten Gang passt (± 5 %; ohne gelernte Gänge entfällt das),
//   die Drehzahl nicht schneller fällt als das Tempo (sonst ausgekuppelt, z. B. beim Schalten)
//   und eine Lambda-Messung, die jünger als 0,5 s ist, nicht fett meldet.
// Eine ältere Lambda-Messung zählt nicht: dann entscheiden Pedal, Gang und Drehzahl allein.
// Ein: erst wenn das 0,4 s am Stück gilt. Aus: sofort (Gas, Gang weg, ausgekuppelt, Drehzahl zu klein).
// Eine feste Obergrenze für Drehzahl oder Tempo gibt es nicht.
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "calc/fuel.h"

namespace fuel {

class CutDetector {
 public:
  struct In {
    Input in;                     // Tempo, Drehzahl, Pedal (mit pedalClosedPct), Klappe, 0x03; o2V wird nicht genutzt
    bool hasFuelSys = false;      // 0x03 unterstützt
    float throttleClosedPct = NAN;
    float o2V = NAN;              // letzte Lambda-Messung (auch alt), NAN = nie
    uint32_t o2AgeMs = UINT32_MAX;
    uint32_t rpmT = 0;            // Zeitstempel der Drehzahl-Messung (neue Probe = neuer Wert)
    const float* gears = nullptr; // gelernte Gänge (k = km/h je 1000 U/min)
    int gearCount = 0;
  };

  // Grund der letzten Entscheidung (Diagnose "Schub")
  enum class Why : uint8_t {
    NoData,      // Tempo oder Drehzahl fehlt
    FuelSys,     // 0x03 = 4
    Slow,        // unter 1200 U/min bzw. 15 km/h
    Gas,         // Fuß auf dem Gas
    NoPedal,     // weder Pedal noch Klappe noch frische Lambda-Messung
    NoGear,      // passt zu keinem gelernten Gang
    Declutched,  // Drehzahl fällt schneller als das Tempo
    Rich,        // frische Lambda-Messung fett
    Waiting,     // alles passt, Einschaltverzug läuft
    Cut,         // Schub
  };

  bool step(const In& x, uint32_t nowMs);
  bool active() const { return active_; }
  // Fuß vom Gas über der Mindestdrehzahl: Lambda bevorzugt abfragen
  bool watch() const { return watch_; }
  Why why() const { return why_; }
  void reset();
  // Diagnose-Zeile, z. B. "ja: Pedal 0 %, Gang 3, Lambda 0,05 V 0,2 s"
  void describe(char* buf, size_t n) const;

 private:
  bool active_ = false;
  bool watch_ = false;
  Why why_ = Why::NoData;
  uint32_t candSince_ = 0;
  // Drehzahl-Proben für "fällt schneller als das Tempo": k = Tempo/Drehzahl je Probe
  static constexpr int N = 8;
  float k_[N] = {};
  uint32_t t_[N] = {};
  int head_ = 0, count_ = 0;
  uint32_t lastRpmT_ = 0;
  // für describe()
  float pedal_ = NAN;
  int gearIdx_ = -1, gearNum_ = 0;
  float slip_ = NAN;
  float o2_ = NAN;
  uint32_t o2Age_ = UINT32_MAX;
  bool pedalIsThrottle_ = false;
};

}  // namespace fuel
