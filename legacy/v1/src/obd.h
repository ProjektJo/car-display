#pragma once
#include <Arduino.h>

// Alle Messwerte, die vom Auto gelesen werden.
enum Metric : uint8_t {
  M_RPM,        // 1/min
  M_SPEED,      // km/h
  M_LOAD,       // %  berechnete Motorlast
  M_THROTTLE,   // %  Drosselklappe
  M_COOLANT,    // °C Kühlmittel (echter Wert, nicht die "Mitte"-Nadel)
  M_INTAKE,     // °C Ansaugluft
  M_VOLT,       // V  Bordspannung (AT RV, direkt am OBD-Stecker)
  M_FUELRATE,   // l/h (PID 0x5E, nicht jedes Auto)
  M_MAF,        // g/s Luftmasse (Ersatz für Verbrauchsberechnung)
  M_FUELLEVEL,  // %  Tankfüllstand
  M_STFT,       // %  Kurzzeit-Gemischkorrektur Bank 1
  M_LTFT,       // %  Langzeit-Gemischkorrektur Bank 1
  M_OIL,        // °C Öltemperatur (PID 0x5C, selten unterstützt)
  M_COUNT
};

enum class LinkState : uint8_t { Connecting, InitElm, Running, Failed };

struct Trip {
  float distanceKm  = 0;
  float fuelL       = 0;   // nur gültig, wenn Verbrauch bestimmbar
  uint32_t driveMs  = 0;   // Zeit mit Motor an
  float maxCoolant  = NAN;
  float maxRpm      = 0;
  float maxSpeed    = 0;
  float minVolt     = NAN; // tiefster Wert bei laufendem Motor
};

struct CarData {
  float    value[M_COUNT];
  bool     valid[M_COUNT];        // mindestens einmal erfolgreich gelesen
  bool     unsupported[M_COUNT];  // Auto liefert den PID nicht
  uint32_t updatedMs[M_COUNT];

  // Abgeleitet
  float litersPerHour   = NAN;
  float litersPer100km  = NAN;   // NAN bei Stillstand
  Trip  trip;

  LinkState link = LinkState::Connecting;
  uint32_t  pollsPerSec = 0;     // Diagnose: wie schnell kommen Antworten
  char      status[48] = "";

  float get(Metric m) const { return valid[m] ? value[m] : NAN; }
  bool  engineRunning() const { return valid[M_RPM] && value[M_RPM] > 300; }
};

namespace obd {
void begin();
// Muss in jedem loop()-Durchlauf aufgerufen werden, blockiert nicht
// (außer beim Bluetooth-Verbindungsaufbau).
void update();
const CarData& data();
void resetTrip();
}  // namespace obd
