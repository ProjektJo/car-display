#include "eco.h"

#include <cstring>

namespace eco {

namespace {
float clamp100(float v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }
const float LOG_RATIO = std::log(cfg::GEAR_BIN_RATIO);
}  // namespace

// ---------------------------------------------------------------------------
// Gänge
// ---------------------------------------------------------------------------

void GearHistogram::clear() {
  memset(bins, 0, sizeof(bins));
  total = 0;
}

void GearHistogram::add(float k) {
  if (!(k >= cfg::GEAR_K_MIN)) return;
  const int i = static_cast<int>(std::log(k / cfg::GEAR_K_MIN) / LOG_RATIO);
  if (i < 0 || i >= cfg::GEAR_BINS) return;
  // Läuft eine Klasse voll, alle halbieren: alte Fahrten zählen dann weniger, die Form bleibt
  if (bins[i] == UINT16_MAX) {
    total = 0;
    for (int j = 0; j < cfg::GEAR_BINS; j++) {
      bins[j] /= 2;
      total += bins[j];
    }
  }
  bins[i]++;
  total++;
}

float ratioK(float speedKmh, float rpm) {
  if (std::isnan(speedKmh) || std::isnan(rpm) || rpm < 1.0f) return NAN;
  return speedKmh / rpm * 1000.0f;
}

int findGears(const GearHistogram& h, float* out, int max) {
  // Geglättet über ± 3 Klassen (± 3 %), damit eine Häufung eine Spitze ergibt
  static constexpr int W = 3;
  uint32_t smooth[cfg::GEAR_BINS];
  for (int i = 0; i < cfg::GEAR_BINS; i++) {
    uint32_t s = 0;
    for (int j = i - W; j <= i + W; j++)
      if (j >= 0 && j < cfg::GEAR_BINS) s += h.bins[j];
    smooth[i] = s;
  }
  const uint32_t minCount = static_cast<uint32_t>(h.total * cfg::GEAR_PEAK_MIN_SHARE);
  const uint32_t need = minCount > cfg::GEAR_PEAK_MIN_SAMPLES ? minCount : cfg::GEAR_PEAK_MIN_SAMPLES;
  // Spitzen nach Höhe: die höchste zuerst, Nachbarn innerhalb von 12 % fallen weg
  const int sepBins = static_cast<int>(std::ceil(std::log(cfg::GEAR_PEAK_MIN_SEPARATION) / LOG_RATIO));
  bool taken[cfg::GEAR_BINS] = {};
  float found[16];
  int n = 0;
  while (n < 16) {
    int best = -1;
    for (int i = 0; i < cfg::GEAR_BINS; i++)
      if (!taken[i] && (best < 0 || smooth[i] > smooth[best])) best = i;
    if (best < 0 || smooth[best] < need) break;
    for (int j = best - sepBins + 1; j < best + sepBins; j++)
      if (j >= 0 && j < cfg::GEAR_BINS) taken[j] = true;
    // Schwerpunkt der Klassen um die Spitze = genauer k-Wert
    double sumW = 0, sumK = 0;
    for (int j = best - W; j <= best + W; j++) {
      if (j < 0 || j >= cfg::GEAR_BINS) continue;
      const double kMid = cfg::GEAR_K_MIN * std::exp((j + 0.5) * LOG_RATIO);
      sumW += h.bins[j];
      sumK += h.bins[j] * kMid;
    }
    if (sumW > 0) found[n++] = static_cast<float>(sumK / sumW);
  }
  // aufsteigend sortieren
  for (int i = 1; i < n; i++)
    for (int j = i; j > 0 && found[j] < found[j - 1]; j--) {
      const float t = found[j];
      found[j] = found[j - 1];
      found[j - 1] = t;
    }
  const int count = n < max ? n : max;
  for (int i = 0; i < count; i++) out[i] = found[i];
  return count;
}

int matchGear(const float* gears, int count, float k) {
  if (std::isnan(k)) return -1;
  int best = -1;
  float bestDev = cfg::GEAR_MATCH_TOL;
  for (int i = 0; i < count; i++) {
    if (!(gears[i] > 0)) continue;
    const float dev = std::fabs(k / gears[i] - 1.0f);
    if (dev <= bestDev) {
      bestDev = dev;
      best = i;
    }
  }
  return best;
}

int gearNumber(const float* gears, int count, int index) {
  if (index < 0 || index >= count) return GEAR_NONE;
  return index + 1 + (gears[0] > cfg::GEAR_FIRST_MAX_K ? 1 : 0);
}

bool StableK::add(float k, uint32_t nowMs) {
  if (std::isnan(k)) {
    count_ = 0;
    return false;
  }
  head_ = (head_ + 1) % N;
  k_[head_] = k;
  t_[head_] = nowMs;
  if (count_ < N) count_++;
  // Älteste Probe im Fenster suchen; alle Proben seit 1,5 s müssen in 3 % liegen
  float lo = k, hi = k;
  uint32_t span = 0;
  const uint32_t windowMs = static_cast<uint32_t>(cfg::GEAR_STABLE_WINDOW_S * 1000.0f);
  for (int n = 1; n < count_; n++) {
    const int i = (head_ - n + N) % N;
    const uint32_t age = nowMs - t_[i];
    if (k_[i] < lo) lo = k_[i];
    if (k_[i] > hi) hi = k_[i];
    span = age;
    if (age >= windowMs) break;
  }
  return span >= windowMs && hi <= lo * (1.0f + cfg::GEAR_STABLE_MAX_CHANGE);
}

int8_t displayGear(const float* gears, int count, float speedKmh, float rpm, int& index) {
  index = -1;
  if (std::isnan(rpm) || rpm < cfg::ENGINE_RUNNING_MIN_RPM || std::isnan(speedKmh)) return GEAR_NONE;
  if (speedKmh < cfg::MOVING_MIN_KMH) return GEAR_NEUTRAL;  // Stand
  const float k = ratioK(speedKmh, rpm);
  index = matchGear(gears, count, k);
  if (index >= 0) return static_cast<int8_t>(gearNumber(gears, count, index));
  if (rpm < cfg::GEAR_IDLE_MAX_RPM && speedKmh > cfg::GEAR_NEUTRAL_MIN_KMH) return GEAR_NEUTRAL;
  return GEAR_NONE;
}

bool shiftAdvice(float rpm, float pedalPct, bool pedalClosed, bool fuelCut, const float* gears, int count,
                 int index, float shiftRpm, float nextMinRpm) {
  if (std::isnan(rpm) || rpm < shiftRpm || fuelCut || pedalClosed) return false;
  if (std::isnan(pedalPct) || pedalPct >= cfg::SHIFT_MAX_PEDAL_PCT) return false;
  if (count <= 0) return true;  // ohne gelernte Gänge nur nach Drehzahl (A6)
  if (index < 0) return false;  // Kupplung getreten bzw. Schalten
  if (index >= count - 1) return false;       // höchster Gang
  return rpm * gears[index] / gears[index + 1] >= nextMinRpm;
}

// ---------------------------------------------------------------------------
// Fahrphysik
// ---------------------------------------------------------------------------

float roadLoadN(float massKg, float cwA, float vMs) {
  return 0.5f * cfg::AIR_DENSITY * cwA * vMs * vMs + cfg::ROLL_COEFF * massKg * cfg::GRAVITY;
}

float wheelPowerW(float massKg, float cwA, float vMs, float accelMs2) {
  if (std::isnan(vMs) || std::isnan(accelMs2)) return NAN;
  return (massKg * accelMs2 + roadLoadN(massKg, cwA, vMs)) * vMs;
}

float brakePowerW(float massKg, float cwA, float vMs, float accelMs2) {
  if (std::isnan(vMs) || std::isnan(accelMs2)) return 0;
  const float f = -massKg * accelMs2 - roadLoadN(massKg, cwA, vMs);
  return f > 0 ? f * vMs : 0;
}

float litersFromJoule(double joule, float heatMjPerL) {
  return static_cast<float>(joule / (cfg::ENGINE_EFFICIENCY * heatMjPerL * 1e6));
}

// ---------------------------------------------------------------------------
// Eco-Score
// ---------------------------------------------------------------------------

ScoreParts score(const ScoreInput& in) {
  ScoreParts p;
  const float mv = in.moveS > 0 ? in.moveS : 0;
  if (mv <= 0 || in.driveS <= 0) {
    p.roll = p.calm = p.early = p.brake = p.idle = p.score = NAN;
    return p;
  }
  p.roll = clamp100(in.rollS / mv / cfg::SCORE_ROLL_FULL_SHARE * 100.0f);
  p.calm = clamp100(100.0f - cfg::SCORE_CALM_PER_PCT_S * (in.pedalAbs / mv - 1.0f));
  p.early = clamp100(100.0f - cfg::SCORE_EARLY_FACTOR * in.shiftOpenS / mv);
  const float brakeL100 = in.km > 0 ? in.brakedL / in.km * 100.0f : 0.0f;
  p.brake = clamp100(100.0f * (1.0f - brakeL100 / cfg::SCORE_BRAKE_ZERO_L100));
  p.idle = clamp100(100.0f * (1.0f - in.idleS / in.driveS / cfg::SCORE_IDLE_ZERO_SHARE));
  const float s = cfg::SCORE_W_ROLL * p.roll + cfg::SCORE_W_CALM * p.calm + cfg::SCORE_W_EARLY * p.early +
                  cfg::SCORE_W_BRAKE * p.brake + cfg::SCORE_W_IDLE * p.idle;
  p.score = std::round(s);
  return p;
}

// ---------------------------------------------------------------------------
// Fahrzeug-Prüfung
// ---------------------------------------------------------------------------

void GearCheck::reset() { stableS_ = matchedS_ = 0; }

void GearCheck::add(bool matched, float dtS) {
  stableS_ += dtS;
  if (matched) matchedS_ += dtS;
}

bool GearCheck::mismatch() const {
  return stableS_ >= cfg::GEAR_CHECK_MIN_STABLE_S && matchedS_ <= stableS_ * cfg::GEAR_CHECK_MAX_MATCH_SHARE;
}

// ---------------------------------------------------------------------------
// Spartipps
// ---------------------------------------------------------------------------

void TipEngine::reset() { *this = TipEngine(); }

bool TipEngine::cause(Tip t, const TipInput& in) const {
  const uint32_t now = in.nowMs;
  switch (t) {
    case Tip::Coast: return coastSince_ && now - coastSince_ >= cfg::TIP_COAST_HOLD_MS;
    case Tip::HardPedal: return hardSince_ && now - hardSince_ >= cfg::TIP_HARD_HOLD_MS;
    case Tip::LateLift:
      return !std::isnan(in.accelMs2) && in.accelMs2 < -cfg::TIP_LATE_DECEL_MS2 && pedalHighAt_ &&
             now - pedalHighAt_ <= cfg::TIP_LATE_WINDOW_MS;
    case Tip::Idle: return standSince_ && now - standSince_ >= cfg::TIP_IDLE_AFTER_MS;
    case Tip::Steady: return steadyNow_;
    case Tip::Tempo:
      return fastSince_ && now - fastSince_ >= cfg::TIP_TEMPO_HOLD_MS && !std::isnan(in.tempoSaveL) && in.tempoSaveL > 0;
    default: return false;
  }
}

// Anlass vorbei: Hinweis sofort weg (A9)
bool TipEngine::gone(Tip t, const TipInput& in) const {
  switch (t) {
    case Tip::Coast:
      return in.gear != GEAR_NEUTRAL || std::isnan(in.accelMs2) || in.accelMs2 > -cfg::TIP_COAST_END_DECEL_MS2;
    case Tip::HardPedal: return std::isnan(in.pedalPct) || in.pedalPct <= cfg::TIP_HARD_PEDAL_PCT;
    case Tip::Idle: return standSince_ == 0;
    case Tip::Steady: return !steadyNow_;
    case Tip::Tempo: return fastSince_ == 0;
    default: return false;
  }
}

bool TipEngine::ready(Tip t, uint32_t nowMs) const {
  const uint32_t last = lastShown_[static_cast<int>(t)];
  if (t == Tip::Idle) {
    // Stand-Hinweis: alle 30 s wieder, solange das Auto steht (eigener Rhythmus, ohne die 60-s-Sperre)
    return last == 0 || last < standSince_ || nowMs - last >= cfg::TIP_IDLE_REPEAT_MS;
  }
  if (lastAnyTip_ && nowMs - lastAnyTip_ < cfg::TIP_GAP_MS) return false;
  return last == 0 || nowMs - last >= cfg::TIP_SAME_GAP_MS;
}

Tip TipEngine::update(const TipInput& in) {
  const uint32_t now = in.nowMs ? in.nowMs : 1;
  const bool moving = !std::isnan(in.speedKmh) && in.speedKmh >= cfg::TIP_STAND_MAX_KMH;

  // Zustände der Regeln nachführen
  const bool decel = !std::isnan(in.accelMs2) && in.accelMs2 < -cfg::TIP_COAST_DECEL_MS2;
  const bool coasting = in.engineOn && in.gear == GEAR_NEUTRAL && !std::isnan(in.speedKmh) &&
                        in.speedKmh > cfg::TIP_COAST_MIN_KMH && decel;
  coastSince_ = coasting ? (coastSince_ ? coastSince_ : now) : 0;
  // Mit MPU6050: Überholen (hohe Längsbeschleunigung) und Bergauffahrt lösen nichts aus (A9)
  const bool excused = (!std::isnan(in.imuLongMs2) && in.imuLongMs2 > cfg::IMU_OVERTAKE_MS2) ||
                       (!std::isnan(in.slopePct) && in.slopePct > cfg::SLOPE_UPHILL_PCT);
  const bool hard = in.engineOn && !excused && !std::isnan(in.pedalPct) && in.pedalPct > cfg::TIP_HARD_PEDAL_PCT &&
                    !std::isnan(in.speedKmh) && in.speedKmh > cfg::TIP_HARD_MIN_KMH;
  hardSince_ = hard ? (hardSince_ ? hardSince_ : now) : 0;
  if (!std::isnan(in.pedalPct) && in.pedalPct > cfg::TIP_LATE_PEDAL_PCT) pedalHighAt_ = now;
  const bool standing = in.engineOn && !std::isnan(in.speedKmh) && !moving;
  standSince_ = standing ? (standSince_ ? standSince_ : now) : 0;
  const bool fast = in.engineOn && !std::isnan(in.speedKmh) && in.speedKmh > cfg::TIP_TEMPO_KMH;
  fastSince_ = fast ? (fastSince_ ? fastSince_ : now) : 0;

  // Gleichmäßig: Tempo und Pedal der letzten 10 s
  steadyNow_ = false;
  if (in.engineOn && !std::isnan(in.speedKmh) && !std::isnan(in.pedalPct)) {
    stHead_ = (stHead_ + 1) % STEADY_N;
    stSpeed_[stHead_] = in.speedKmh;
    stPedal_[stHead_] = in.pedalPct;
    stT_[stHead_] = now;
    if (stCount_ < STEADY_N) stCount_++;
    float vMin = in.speedKmh, vMax = in.speedKmh, sum = 0, sum2 = 0;
    int n = 0;
    uint32_t span = 0;
    for (int i = 0; i < stCount_; i++) {
      const int j = (stHead_ - i + STEADY_N) % STEADY_N;
      const uint32_t age = now - stT_[j];
      if (age > cfg::TIP_STEADY_WINDOW_MS) break;
      span = age;
      if (stSpeed_[j] < vMin) vMin = stSpeed_[j];
      if (stSpeed_[j] > vMax) vMax = stSpeed_[j];
      sum += stPedal_[j];
      sum2 += stPedal_[j] * stPedal_[j];
      n++;
    }
    if (n > 1 && span + 1000 >= cfg::TIP_STEADY_WINDOW_MS && vMin >= cfg::TIP_STEADY_MIN_KMH &&
        vMax - vMin <= 2 * cfg::TIP_STEADY_SPEED_BAND_KMH) {
      const float mean = sum / n;
      const float var = sum2 / n - mean * mean;
      steadyNow_ = var > cfg::TIP_STEADY_PEDAL_SD * cfg::TIP_STEADY_PEDAL_SD;
    }
  } else {
    stCount_ = 0;
  }

  // Aktiver Tipp: weg nach 20 s bzw. ab 10 s, wenn der Anlass vorbei ist, bei Schub oder ohne Motor
  if (active_ != Tip::None && active_ != Tip::Cold) {
    // Tipps stehen länger (fester Platz, Jos Wunsch): Fahrtipps mindestens TIP_MIN_SHOW_MS, auch wenn der Anlass
    // vorbei ist. "Gang rein" und "Langer Stand" gehen sofort, wenn der Gang drin ist bzw. das Auto fährt.
    const bool sticky = active_ != Tip::Coast && active_ != Tip::Idle;
    if (now - shownAt_ >= cfg::TIP_SHOW_MS || ((!sticky || now - shownAt_ >= cfg::TIP_MIN_SHOW_MS) && gone(active_, in)) ||
        in.fuelCut || !in.engineOn)
      active_ = Tip::None;
  }
  if (active_ == Tip::Cold) active_ = Tip::None;  // wird unten neu bewertet

  // Thermostat (einmalig, A9): erscheint, sobald kein anderer Tipp steht, auch mit abgeschalteten Tipps
  if (!thermoInit_) {
    thermoInit_ = true;
    seenThermo_ = in.thermoSeq;
  }
  if (in.thermoSeq != seenThermo_) {
    seenThermo_ = in.thermoSeq;
    thermoPending_ = true;
  }
  if (thermoPending_ && active_ == Tip::None && !in.fuelCut && !in.quiet && in.engineOn) {
    thermoPending_ = false;
    active_ = Tip::Thermo;
    shownAt_ = now;
    lastAnyTip_ = now;
  }

  // Neuer Tipp: nicht im Schub, nicht in Ruhezeiten, nur einer gleichzeitig
  if (active_ == Tip::None && in.tipsEnabled && !in.fuelCut && !in.quiet && in.engineOn) {
    static constexpr Tip ORDER[] = {Tip::Coast, Tip::LateLift, Tip::HardPedal, Tip::Idle, Tip::Steady, Tip::Tempo};
    for (Tip t : ORDER) {
      if (!cause(t, in) || !ready(t, now)) continue;
      active_ = t;
      shownAt_ = now;
      lastShown_[static_cast<int>(t)] = now;
      lastAnyTip_ = now;
      break;
    }
  }

  // Ohne Tipp: Kalter Motor, solange die Bedingung gilt (auch mit abgeschalteten Tipps, A9)
  if (active_ == Tip::None && in.engineOn && !in.fuelCut && !std::isnan(in.coolantC) && !std::isnan(in.rpm) &&
      in.coolantC < in.coldCoolantC && in.rpm > in.coldRpmLimit)
    active_ = Tip::Cold;
  return active_;
}

float tripAxisPos(float tripKm, float fillKm) {
  if (std::isnan(tripKm) || tripKm < cfg::TRIP_AVG_MIN_KM) return NAN;
  if (tripKm <= 1.0f) return 3.0f;
  if (tripKm <= 100.0f) return 3.0f - std::log10(tripKm);
  if (std::isnan(fillKm) || fillKm <= 100.0f) return 1.0f;  // Tank-km unbekannt: auf 100 km
  if (tripKm >= fillKm) return 0.0f;
  return 1.0f - (std::log10(tripKm) - 2.0f) / (std::log10(fillKm) - 2.0f);
}

}  // namespace eco
