// Fahrzeugprofil (A6): alles, was die Rechnung über ein Auto wissen muss, und woran es
// wiedererkannt wird (VIN, PID-Liste, Protokoll). Gespeichert als JSON in /profiles/ (storage).
// Reines C++, damit Rechenmodule und Tests es ohne Arduino nutzen können.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "config.h"

enum class FuelType : uint8_t { Petrol, Diesel };

constexpr uint8_t MAX_GEARS = 8;

struct Profile {
  uint8_t id = 0;               // 1 … MAX_PROFILES, 0 = kein Profil
  char name[33] = "";            // höchstens 16 Zeichen (UTF-8)
  // Wiedererkennung (A6 "Profil erkennen")
  char vin[18] = "";            // leer = Auto liefert keine VIN
  uint8_t supported[32] = {};   // Bitfeld der unterstützten Mode-01-PIDs
  int8_t protocol = -1;         // ELM-Protokollnummer (ATDPN), -1 = unbekannt
  // Motor und Tank
  FuelType fuel = FuelType::Petrol;
  float displacementL = cfg::DEFAULT_DISPLACEMENT_L;
  float tankL = cfg::DEFAULT_TANK_L;
  float reserveL = NAN;         // nicht nutzbare Restmenge (Pumpe), NAN = pauschal aus der Tankgröße
  float ve = cfg::DEFAULT_VE;
  float fuelCal = cfg::DEFAULT_FUEL_CAL;
  float kmFactor = cfg::DEFAULT_KM_FACTOR;
  // Gänge (gelernt ab Etappe 4), k = km/h je 1000 U/min
  float gears[MAX_GEARS] = {};
  uint8_t gearCount = 0;
  // Grenzen
  uint16_t coldRpmLimit = cfg::DEFAULT_COLD_RPM_LIMIT;
  uint8_t coldCoolantC = cfg::DEFAULT_COLD_COOLANT_C;
  uint8_t body = 0;             // Index in cfg::BODY_TYPES
  uint16_t powerKw = 55;
  uint16_t redlineRpm = cfg::DEFAULT_REDLINE_RPM;
  uint16_t shiftRpm = cfg::SHIFT_RPM_PETROL;

  // Werte, die vom Kraftstoff und Hubraum abhängen, neu setzen (Assistent "Neues Fahrzeug")
  void applyDerivedDefaults() {
    shiftRpm = fuel == FuelType::Diesel ? cfg::SHIFT_RPM_DIESEL : cfg::SHIFT_RPM_PETROL;
    powerKw = static_cast<uint16_t>(displacementL * cfg::POWER_KW_PER_L + 0.5f);
  }

  // Reserve: eingetragen oder pauschal 4 % des Tanks (1–3 l)
  float reserve() const {
    if (reserveL >= 0) return reserveL;
    const float r = tankL * cfg::RESERVE_FRAC;
    return r < cfg::RESERVE_MIN_L ? cfg::RESERVE_MIN_L : (r > cfg::RESERVE_MAX_L ? cfg::RESERVE_MAX_L : r);
  }

  bool pidSupported(uint8_t pid) const { return (supported[pid / 8] >> (pid % 8)) & 1; }
  const cfg::BodyType& bodyType() const { return cfg::BODY_TYPES[body < cfg::BODY_TYPE_COUNT ? body : 0]; }
};

// Kurzfassung für die Auswahl "Welches Fahrzeug?" und die Erkennung
struct ProfileSummary {
  uint8_t id = 0;
  char name[33] = "";
  char vin[18] = "";
  uint8_t supported[32] = {};
  int8_t protocol = -1;
  FuelType fuel = FuelType::Petrol;
  float displacementL = 0;

  static ProfileSummary of(const Profile& p) {
    ProfileSummary s;
    s.id = p.id;
    snprintf(s.name, sizeof(s.name), "%s", p.name);
    snprintf(s.vin, sizeof(s.vin), "%s", p.vin);
    memcpy(s.supported, p.supported, sizeof(s.supported));
    s.protocol = p.protocol;
    s.fuel = p.fuel;
    s.displacementL = p.displacementL;
    return s;
  }
};
