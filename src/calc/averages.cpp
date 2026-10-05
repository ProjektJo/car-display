#include "averages.h"

#include "config.h"

namespace avg {

void DistanceRing::init(uint16_t slotCount, float slotLengthM) {
  slots = slotCount > RING_MAX ? RING_MAX : slotCount;
  slotM = slotLengthM;
  head = 0;
  for (uint16_t i = 0; i < RING_MAX; i++) m[i] = ml[i] = 0;
}

void DistanceRing::add(float dm, float dml) {
  if (!(dm > 0) || slots == 0) {
    // Sprit im Stand gehört zum laufenden Abschnitt (Leerlauf zählt mit)
    if (dml > 0 && slots) ml[head] += dml;
    return;
  }
  // Läuft der Abschnitt voll, geht der Rest anteilig in den nächsten (der älteste fällt heraus)
  while (dm > 0) {
    const float room = slotM - m[head];
    if (dm < room) {
      m[head] += dm;
      ml[head] += dml;
      return;
    }
    const float share = room / dm;
    m[head] += room;
    ml[head] += dml * share;
    dm -= room;
    dml -= dml * share;
    head = static_cast<uint16_t>((head + 1) % slots);
    m[head] = 0;
    ml[head] = 0;
  }
}

float DistanceRing::sumM() const {
  float s = 0;
  for (uint16_t i = 0; i < slots; i++) s += m[i];
  return s;
}

float DistanceRing::sumMl() const {
  float s = 0;
  for (uint16_t i = 0; i < slots; i++) s += ml[i];
  return s;
}

float DistanceRing::l100() const {
  const float meters = sumM();
  if (meters < cfg::AVG_MIN_FRACTION * slots * slotM || meters <= 0) return NAN;
  return sumMl() / meters * 100.0f;  // ml/m · 100 = l/100 km
}

float simpleL100(float km, float liters) {
  if (!(km >= cfg::AVG_SIMPLE_MIN_KM)) return NAN;
  return liters / km * 100.0f;
}

float tankL100(float kmSinceFill, float litersSinceFill, float prevFillL100) {
  if (kmSinceFill < cfg::TANK_AVG_PREV_KM && !std::isnan(prevFillL100)) return prevFillL100;
  return simpleL100(kmSinceFill, litersSinceFill);
}

void FillHistory::clear() {
  for (uint8_t i = 0; i < SIZE; i++) km[i] = liters[i] = 0;
  count = 0;
  next = 0;
}

void FillHistory::push(float kmValue, float litersValue) {
  km[next] = kmValue;
  liters[next] = litersValue;
  next = static_cast<uint8_t>((next + 1) % SIZE);
  if (count < SIZE) count++;
}

float FillHistory::l100() const {
  float k = 0, l = 0;
  for (uint8_t i = 0; i < count; i++) {
    k += km[i];
    l += liters[i];
  }
  return k > 0 ? l / k * 100.0f : NAN;
}

}  // namespace avg
