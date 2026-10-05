#include "profile_match.h"

#include <cstring>

bool pidsMatch(const ProfileSummary& p, const uint8_t supported[32], int8_t protocol) {
  if (p.vin[0]) return false;  // Profil mit VIN passt nur über die VIN (anderes Auto)
  if (memcmp(p.supported, supported, sizeof(p.supported)) != 0) return false;
  return p.protocol < 0 || protocol < 0 || p.protocol == protocol;
}

bool profileFitsCar(const ProfileSummary& p, const char* vin, const uint8_t supported[32], int8_t protocol) {
  if (vin && vin[0] && strcmp(p.vin, vin) == 0) return true;
  return pidsMatch(p, supported, protocol);
}

ProfileMatch matchProfile(const ProfileSummary* list, int count, const char* vin, const uint8_t supported[32],
                          int8_t protocol) {
  ProfileMatch m;
  const bool carVin = vin && vin[0];

  // 1. VIN: eindeutig, gewinnt immer
  if (carVin) {
    for (int i = 0; i < count; i++) {
      if (strcmp(list[i].vin, vin) == 0) {
        m.kind = ProfileMatch::Kind::One;
        m.id = list[i].id;
        return m;
      }
    }
  }

  // 2. PID-Liste und Protokoll genau gleich. Ein Profil mit einer anderen VIN passt nie;
  //    ein Profil mit VIN passt auch nicht zu einem Auto ohne VIN (anderes Auto).
  int hits = 0;
  for (int i = 0; i < count; i++) {
    const ProfileSummary& p = list[i];
    if (!pidsMatch(p, supported, protocol)) continue;  // eine VIN am Profil war oben nicht gleich
    if (hits == 0) m.id = p.id;
    hits++;
  }
  if (hits == 1) {
    m.kind = ProfileMatch::Kind::One;
    m.learnVin = carVin;
  } else {
    m.kind = hits == 0 ? ProfileMatch::Kind::None : ProfileMatch::Kind::Many;
    m.id = 0;
  }
  return m;
}
