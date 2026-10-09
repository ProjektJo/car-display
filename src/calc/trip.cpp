#include "trip.h"

#include <cstring>

#include "config.h"

namespace trip {

void start(TripState& t, uint16_t number) {
  memset(&t, 0, sizeof(t));
  t.active = 1;
  t.number = number;
  t.maxCoolantC = NAN;
  t.ecoScore = NAN;
}

TripRecord toRecord(const TripState& t, uint8_t profileId) {
  TripRecord r;
  memset(&r, 0, sizeof(r));
  r.number = t.number;
  r.profileId = profileId;
  r.km = static_cast<float>(t.km);
  r.liters = static_cast<float>(t.liters);
  r.durationS = static_cast<float>(t.durationS);
  r.idleS = static_cast<float>(t.idleS);
  r.cutS = static_cast<float>(t.cutS);
  r.brakedL = t.brakedL;
  r.ecoScore = t.ecoScore;
  r.maxRpm = t.maxRpm;
  r.maxCoolantC = t.maxCoolantC;
  r.cost = t.costKnown ? static_cast<float>(t.cost) : NAN;
  return r;
}

bool continues(float coolantAtStopC, float coolantAtStartC) {
  if (std::isnan(coolantAtStartC)) return false;
  if (coolantAtStartC >= cfg::TRIP_WARM_C) return true;
  return !std::isnan(coolantAtStopC) && coolantAtStartC >= coolantAtStopC - cfg::TRIP_PAUSE_MAX_DROP_C;
}

}  // namespace trip
