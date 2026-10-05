// Profil zum verbundenen Auto finden (A6 "Profil erkennen", Jos Entscheidung vom 5. Oktober):
// erst über die VIN, sonst über PID-Liste und Protokoll. Keine Bluetooth-Adresse.
// Reines C++, nativ testbar.
#pragma once
#include <cstdint>

#include "core/profile.h"

struct ProfileMatch {
  enum class Kind : uint8_t { None, One, Many };
  Kind kind = Kind::None;
  uint8_t id = 0;        // bei One
  bool learnVin = false; // Auto liefert eine VIN, das Profil kennt noch keine: VIN merken
};

// vin leer = Auto liefert keine. protocol -1 = unbekannt.
// Passt ein Profil über PID-Liste und Protokoll (Schritt 2 der Erkennung)? Profile mit VIN nie.
bool pidsMatch(const ProfileSummary& p, const uint8_t supported[32], int8_t protocol);
// Würde das Profil für dieses Auto in Frage kommen (gleiche VIN oder Schritt 2)?
bool profileFitsCar(const ProfileSummary& p, const char* vin, const uint8_t supported[32], int8_t protocol);

ProfileMatch matchProfile(const ProfileSummary* list, int count, const char* vin, const uint8_t supported[32],
                          int8_t protocol);
