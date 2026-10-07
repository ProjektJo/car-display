#include "values.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "ui/symbols.h"
#include "ui/theme.h"
#include "util/format.h"

namespace values {

namespace {

struct Def {
  const char* id;
  const char* label;
  const char* unit;
  int decimals;
  Series series;
};

// Reihenfolge = Key
const Def DEFS[COUNT] = {
    {"speed", "Tempo", "km/h", 0, Series::Speed},
    {"rpm", "Drehzahl", "U/min", 0, Series::Rpm},
    {"inst", "Momentan", "l/100", 1, Series::Inst},
    {"avg10", SYM_AVG " 10 km", "l/100", 1, Series::None},
    {"avg100", SYM_AVG " 100 km", "l/100", 1, Series::None},
    {"avgTank", SYM_AVG " Tank", "l/100", 1, Series::None},
    {"avgAll", SYM_AVG " seit Profil", "l/100", 1, Series::None},
    {"pedal", "Gaspedal", "%", 0, Series::Pedal},
    {"range", "Reichweite", "km", 0, Series::None},
    {"coolant", "Kühlmittel", SYM_DEG "C", 0, Series::Coolant},
    {"volt", "Bordspannung", "V", 1, Series::Volt},
    {"load", "Motorlast", "%", 0, Series::None},
    {"map", "Saugrohr", "kPa", 0, Series::None},
    {"iat", "Ansaugluft", SYM_DEG "C", 0, Series::None},
    {"gear", "Gang", "", 0, Series::None},
    {"eco", "Eco-Score", "", 0, Series::None},
    {"cutSaved", "Schub gespart", "l", 2, Series::None},
    {"braked", "Gebremst", "l", 2, Series::None},
    {"avgTrip", SYM_AVG " Fahrt", "l/100", 1, Series::None},
    {"tripKm", "Strecke", "km", 1, Series::None},
    {"tripTime", "Fahrzeit", "h", 0, Series::None},
    {"avgSpeed", SYM_AVG " Tempo", "km/h", 0, Series::None},
    {"vmax", "Vmax", "km/h", 0, Series::None},
    {"kwPeak", "Spitze", "kW", 0, Series::None},
    {"power", "Leistung", "kW", 0, Series::None},
    {"tankL", "Tankinhalt", "l", 1, Series::None},
    {"tripCost", "Kosten Fahrt", "\xE2\x82\xAC", 2, Series::None},
};

struct SeriesDef {
  const char* label;
  const char* shortLabel;
  bool fromZero;
  int decimals;
};
const SeriesDef SERIES[SERIES_COUNT] = {
    {"Verbrauch", "Verbr.", true, 1}, {"Tempo", "Tempo", true, 0},    {"Drehzahl", "Drehz.", true, 0},
    {"Gaspedal", "Gas", true, 0},     {"Kühlmittel", "Kühlm.", false, 0}, {"Spannung", "Volt", false, 1},
};

const Def& def(Key k) { return DEFS[static_cast<int>(k) < COUNT ? static_cast<int>(k) : 0]; }

}  // namespace

const char* label(Key k) { return def(k).label; }
int decimals(Key k) { return def(k).decimals; }
Series series(Key k) { return def(k).series; }
const char* id(Key k) { return def(k).id; }

bool fromId(const char* s, Key& out) {
  for (int i = 0; i < COUNT; i++)
    if (strcmp(DEFS[i].id, s) == 0) {
      out = static_cast<Key>(i);
      return true;
    }
  return false;
}

const char* unit(Key k, const CarSnapshot& s) {
  // Momentan: l/100 ab 5 km/h, darunter l/h (A7)
  if (k == Key::Inst && std::isnan(s.fuelL100.get(s.now))) return "l/h";
  return def(k).unit;
}

float value(Key k, const CarSnapshot& s) {
  const uint32_t n = s.now;
  switch (k) {
    case Key::Speed: return s.speed.get(n);
    case Key::Rpm: {
      const float r = s.rpm.get(n);
      return std::isnan(r) ? NAN : std::round(r / 10.0f) * 10.0f;  // ruhiger: auf 10 U/min
    }
    case Key::Inst: {
      if (s.fuelCut && s.engineRunning()) return 0.0f;
      const float l100 = s.fuelL100.get(n);
      return std::isnan(l100) ? s.fuelLph.get(n) : l100;
    }
    case Key::Avg10: return s.avg10.get(n);
    case Key::Avg100: return s.avg100.get(n);
    case Key::AvgTank: return s.avgTank.get(n);
    case Key::AvgProfile: return s.avgProfile.get(n);
    case Key::Pedal: {
      // ANNAHME: ohne Gaspedal (0x49) die Drosselklappe (0x11), wie der Scheduler (A7)
      const float p = s.pedal.get(n);
      return std::isnan(p) ? s.throttle.get(n) : p;
    }
    case Key::Range: return s.rangeKm.get(n);
    case Key::Coolant: return s.coolant.get(n);
    case Key::Volt: return s.voltage.get(n);
    case Key::Load: return s.load.get(n);
    case Key::Map: return s.map.get(n);
    case Key::Iat: return s.iat.get(n);
    case Key::Gear: return s.gear;
    case Key::EcoScore: return s.ecoScore.get(n);
    case Key::CutSaved: return s.cutSavedL.get(n);
    case Key::Braked: return s.brakedL.get(n);
    case Key::AvgTrip: return s.avgTrip.get(n);
    case Key::TripKm: return s.tripKm.get(n);
    case Key::TripTime: return s.tripDurationS.get(n);
    case Key::AvgSpeed: {
      // Ø Tempo der Fahrt = Strecke ÷ Fahrzeit, erst ab 1 min
      const float km = s.tripKm.get(n), dur = s.tripDurationS.get(n);
      return (!std::isnan(km) && !std::isnan(dur) && dur > 60) ? km / (dur / 3600.0f) : NAN;
    }
    case Key::Vmax: return s.tripVmax.get(n);
    case Key::KwPeak: return s.tripKwPeak.get(n);
    case Key::Power: return s.powerKw.get(n);
    case Key::TankL: return s.tankL.get(n);
    case Key::TripCost: return s.tripCost.get(n);
    default: return NAN;
  }
}

void text(Key k, const CarSnapshot& s, char* out, size_t size) {
  if (k == Key::Gear) {
    if (s.gear < 0 || !s.engineRunning())
      snprintf(out, size, "%s", fmt::NO_VALUE);
    else if (s.gear == 0)
      snprintf(out, size, "N");
    else
      snprintf(out, size, "%d", s.gear);
    return;
  }
  if (k == Key::TripTime) {  // h:mm
    const float sec = value(k, s);
    if (std::isnan(sec)) {
      snprintf(out, size, "%s", fmt::NO_VALUE);
    } else {
      const unsigned m = static_cast<unsigned>(sec / 60);
      snprintf(out, size, "%u:%02u", m / 60, m % 60);
    }
    return;
  }
  fmt::number(out, size, value(k, s), decimals(k));
}

float consumptionRef(const CarSnapshot& s) {
  const float goal = s.goalL100.get(s.now);
  return std::isnan(goal) ? s.avgTank.get(s.now) : goal;
}

uint32_t consumptionColor(float l100, const CarSnapshot& s) {
  const float ref = consumptionRef(s);
  if (std::isnan(l100) || std::isnan(ref) || ref <= 0) return theme::TEXT;
  if (l100 < ref * cfg::COLOR_GOOD_BELOW) return theme::GOOD;
  if (l100 > ref * cfg::COLOR_WARN_ABOVE) return theme::WARN;
  return theme::TEXT;
}

uint32_t color(Key k, const CarSnapshot& s) {
  switch (k) {
    case Key::Rpm: {
      // Kalter Motor: Drehzahl über der Kalt-Grenze (A9)
      const float c = s.coolant.get(s.now), r = s.rpm.get(s.now);
      return (!std::isnan(c) && !std::isnan(r) && c < s.profile.coldCoolantC && r > s.profile.coldRpmLimit) ? theme::WARN
                                                                                                          : theme::TEXT;
    }
    case Key::Inst:
      if (s.fuelCut && s.engineRunning()) return theme::GOOD;
      return consumptionColor(s.fuelL100.get(s.now), s);
    case Key::Range: {
      const float r = s.rangeKm.get(s.now);
      return (!std::isnan(r) && r < cfg::RANGE_LOW_KM) ? theme::WARN : theme::TEXT;
    }
    case Key::AvgTrip: return consumptionColor(s.avgTrip.get(s.now), s);
    case Key::EcoScore: {
      const float e = s.ecoScore.get(s.now);
      if (std::isnan(e)) return theme::TEXT;
      return e >= cfg::SCORE_GOOD ? theme::GOOD : (e < cfg::SCORE_OK ? theme::WARN : theme::TEXT);
    }
    case Key::CutSaved: return theme::GOOD;
    default: return theme::TEXT;
  }
}

float seriesValue(Series r, const CarSnapshot& s) {
  switch (r) {
    case Series::Inst: {
      // Verbrauch: l/100 km (Schub 0), im Stand kein Wert; auf 20 begrenzt wie in der Vorschau
      if (!s.engineRunning()) return NAN;
      if (s.fuelCut) return 0.0f;
      const float v = s.fuelL100.get(s.now);
      return std::isnan(v) ? NAN : (v > 20.0f ? 20.0f : v);
    }
    case Series::Speed: return s.speed.get(s.now);
    case Series::Rpm: return s.rpm.get(s.now);
    case Series::Pedal: return value(Key::Pedal, s);
    case Series::Coolant: return s.coolant.get(s.now);
    case Series::Volt: return s.voltage.get(s.now);
    default: return NAN;
  }
}

const char* seriesLabel(Series r) { return r >= Series::Inst && r < Series::COUNT ? SERIES[static_cast<int>(r)].label : ""; }
const char* seriesShort(Series r) {
  return r >= Series::Inst && r < Series::COUNT ? SERIES[static_cast<int>(r)].shortLabel : "";
}
bool seriesFromZero(Series r) { return r >= Series::Inst && r < Series::COUNT && SERIES[static_cast<int>(r)].fromZero; }
int seriesDecimals(Series r) { return r >= Series::Inst && r < Series::COUNT ? SERIES[static_cast<int>(r)].decimals : 0; }

}  // namespace values
