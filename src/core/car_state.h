// CarState: alle Fahrzeugwerte mit Zeitstempel je Wert (A5, M Qualitätsregeln).
//
// Datenfluss: obdTask (bzw. Simulator) und calcTask schreiben unter einem Mutex,
// uiTask holt sich 10-mal pro Sekunde eine Kopie (CarSnapshot). Nur uiTask ruft LVGL auf.
// Die Strukturen sind reines C++, damit Rechenmodule und Tests sie ohne Arduino nutzen können.
#pragma once
#include <cmath>
#include <cstdint>

#include "config.h"

// Ein Messwert mit Zeitstempel. Ohne gültige, aktuelle Messung liefert get() NAN,
// die UI zeigt dann "–" statt einer eingefrorenen Zahl.
struct Val {
  float v = NAN;
  uint32_t t = 0;          // Zeitpunkt der letzten Messung in ms (millis), 0 = noch nie
  uint32_t periodMs = 0;   // planmäßiger Abfragetakt (cfg::PID_PERIOD_*), bestimmt das Veralten

  void set(float value, uint32_t nowMs) {
    v = value;
    t = nowMs ? nowMs : 1;
  }
  void clear() {
    v = NAN;
    t = 0;
  }
  // Aktuell = gemessen und höchstens STALE_GRACE_MS über den nächsten planmäßigen Abfragezeitpunkt hinaus
  bool fresh(uint32_t nowMs) const {
    return t != 0 && !std::isnan(v) && (nowMs - t) <= periodMs + cfg::STALE_GRACE_MS;
  }
  float get(uint32_t nowMs) const { return fresh(nowMs) ? v : NAN; }
};

// Zustand der Verbindung zum Adapter (Startbildschirm und Verbindungspunkt, U Rahmen)
enum class LinkState : uint8_t {
  Off,          // keine Verbindung, kein Versuch
  Searching,    // Suche Adapter
  Connecting,   // BLE-Verbindung wird aufgebaut
  InitAdapter,  // ELM-Init, Protokollsuche
  Running,      // Daten fließen
  Lost,         // Verbindung verloren, warte auf Neuversuch
};

struct CarState {
  // --- Verbindung ---
  LinkState link = LinkState::Off;
  bool simulated = false;

  // --- Rohwerte OBD Mode 01 (PID in Klammern, Takt-Klasse nach A7) ---
  Val speed{NAN, 0, cfg::PID_PERIOD_FAST_MS};          // km/h (0x0D)
  Val rpm{NAN, 0, cfg::PID_PERIOD_FAST_MS};            // U/min (0x0C)
  Val map{NAN, 0, cfg::PID_PERIOD_FAST_MS};            // Saugrohrdruck kPa (0x0B)
  Val throttle{NAN, 0, cfg::PID_PERIOD_FAST_MS};       // Drosselklappe % (0x11)
  Val pedal{NAN, 0, cfg::PID_PERIOD_FAST_MS};          // Gaspedal % (0x49)
  Val iat{NAN, 0, cfg::PID_PERIOD_MEDIUM_MS};          // Ansaugluft °C (0x0F)
  Val stft{NAN, 0, cfg::PID_PERIOD_MEDIUM_MS};         // kurzfristige Gemischkorrektur % (0x06)
  Val ltft{NAN, 0, cfg::PID_PERIOD_MEDIUM_MS};         // langfristige Gemischkorrektur % (0x07)
  Val fuelSys{NAN, 0, cfg::PID_PERIOD_MEDIUM_MS};      // Kraftstoffsystem-Status (0x03), 4 = Schub
  Val load{NAN, 0, cfg::PID_PERIOD_MEDIUM_MS};         // Motorlast % (0x04)
  Val maf{NAN, 0, cfg::PID_PERIOD_MEDIUM_MS};          // Luftmasse g/s (0x10)
  Val fuelRate{NAN, 0, cfg::PID_PERIOD_FAST_MS};       // Kraftstoff l/h (0x5E)
  Val lambdaCmd{NAN, 0, cfg::PID_PERIOD_MEDIUM_MS};    // Soll-Lambda (0x44)
  Val coolant{NAN, 0, cfg::PID_PERIOD_SLOW_MS};        // Kühlmittel °C (0x05)
  Val voltage{NAN, 0, cfg::PID_PERIOD_SLOW_MS};        // Bordspannung V (ATRV)
  Val fuelLevel{NAN, 0, cfg::PID_PERIOD_SLOW_MS};      // Tankfüllstand % (0x2F)
  Val mil{NAN, 0, cfg::PID_PERIOD_RARE_MS};            // Motorkontrollleuchte 0/1 (0x01)
  Val dtcCount{NAN, 0, cfg::PID_PERIOD_RARE_MS};       // Anzahl gespeicherter Fehlercodes (0x01)

  // --- Abgeleitete Werte (calcTask) ---
  Val tankL{NAN, 0, cfg::PID_PERIOD_SLOW_MS};          // Liter im Tank
  Val rangeKm{NAN, 0, cfg::PID_PERIOD_SLOW_MS};        // Reichweite km (ab Etappe 3)

  // --- Optionale Sensoren (Etappe 8) ---
  bool hasImu = false;
  bool hasGps = false;
  bool gpsTimeValid = false;
  uint8_t gpsHour = 0, gpsMinute = 0;                  // Ortszeit
};

// Kopie für die UI, mit dem Zeitpunkt der Kopie
struct CarSnapshot : CarState {
  uint32_t now = 0;    // millis() beim Kopieren; Bezug für Val::fresh()
  uint32_t seq = 0;    // zählt jede Änderung am CarState hoch

  bool engineRunning() const {
    const float r = rpm.get(now);
    return !std::isnan(r) && r > cfg::ENGINE_RUNNING_MIN_RPM;
  }
};
