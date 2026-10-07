#include "perf.h"

#include <cstring>

namespace perf {

void Trace::clear() {
  memset(cs, 0, sizeof(cs));
  count = 0;
}

void Trace::add(float tS, float vKmh) {
  while (count < TRACE_POINTS && vKmh >= count * TRACE_STEP_KMH) {
    const float c = tS * 100.0f;
    cs[count++] = static_cast<uint16_t>(c < 0 ? 0 : (c > 65535 ? 65535 : c + 0.5f));
  }
}

void Best::clear() {
  s50 = s100 = s80120 = NAN;
  trace100.clear();
}

void SprintMeter::reset() {
  const Best b = best_;
  *this = SprintMeter();
  best_ = b;
}

float SprintMeter::elapsed(uint32_t nowMs) const {
  if (state_ == State::Running || state_ == State::Waiting) return (nowMs - startMs_) / 1000.0f;
  if (state_ == State::Done) return last100_;
  return NAN;
}

bool SprintMeter::takeBestChanged() {
  const bool c = bestChanged_;
  bestChanged_ = false;
  return c;
}

void SprintMeter::abort() { state_ = State::Ready; }

void SprintMeter::result(Kind k, float s, float& best) {
  resultKind_ = k;
  resultS_ = s;
  resultPrevBest_ = best;
  resultSeq_++;
  if (std::isnan(best) || s < best) {
    best = s;
    bestChanged_ = true;
  }
}

float SprintMeter::speedAgo(uint32_t t, uint32_t sinceMs) const {
  // jüngste Probe, die mindestens sinceMs alt ist
  for (int k = 1; k <= histCount_; k++) {
    const int i = (histHead_ - k + HIST) % HIST;
    if (t - histT_[i] >= sinceMs) return histV_[i];
  }
  return NAN;
}

void SprintMeter::update(uint32_t t, float v, float pedal, float accel) {
  if (std::isnan(v)) return;
  const bool standing = v < cfg::SPRINT_STAND_KMH;
  standing_ = standing;
  const bool wasStanding = !std::isnan(prevV_) && prevV_ < cfg::SPRINT_STAND_KMH;
  // Zeitpunkt, zu dem das Tempo x zwischen der vorigen und dieser Messung erreicht wurde (linear)
  auto cross = [&](float x) -> uint32_t {
    if (std::isnan(prevV_) || v <= prevV_) return t;
    const float f = (x - prevV_) / (v - prevV_);
    return prevT_ + static_cast<uint32_t>((f < 0 ? 0 : (f > 1 ? 1 : f)) * (t - prevT_));
  };
  const bool pedalHigh = !std::isnan(pedal) && pedal >= cfg::SPRINT_PEDAL_PCT;
  histT_[histHead_] = t;
  histV_[histHead_] = v;
  histHead_ = (histHead_ + 1) % HIST;
  if (histCount_ < HIST) histCount_++;
  // Beschleunigung der letzten 3 s zu schwach (Jo: am Anfang mitzählen, im Verlauf abbrechen)
  const float vAgo = speedAgo(t, cfg::SPRINT_ACCEL_WINDOW_MS);
  const bool weak = !std::isnan(vAgo) &&
                    (v - vAgo) / 3.6f / (cfg::SPRINT_ACCEL_WINDOW_MS / 1000.0f) < cfg::SPRINT_MIN_ACCEL_MS2;

  // --- Sprint aus dem Stand erkennen: ≥ 1 s gestanden, dann spätestens 3 s nach dem Anfahren
  //     Gaspedal ≥ 80 % des Bereichs oder im Schnitt ≥ 9 km/h je s seit dem Anfahren (A10, geändert)
  if (standing) {
    if (!standSince_) standSince_ = t ? t : 1;
    leftAt_ = 0;
    if (t - standSince_ < cfg::SPRINT_STAND_MIN_MS) launchLatched_ = false;
  } else {
    if (standSince_ && prevT_ - standSince_ >= cfg::SPRINT_STAND_MIN_MS) leftAt_ = prevT_ ? prevT_ : 1;
    standSince_ = 0;
  }
  const bool armed = (standing && standSince_ && t - standSince_ >= cfg::SPRINT_STAND_MIN_MS) ||
                     (leftAt_ && t - leftAt_ <= cfg::SPRINT_ARM_AFTER_LEAVE_MS);
  const bool strong = leftAt_ && t - leftAt_ >= cfg::SPRINT_LAUNCH_MIN_MS &&
                      v / ((t - leftAt_) / 1000.0f) >= cfg::SPRINT_LAUNCH_KMH_S;
  const bool launch = armed && (pedalHigh || strong);
  if (launch && !launchLatched_) {
    launchLatched_ = true;
    launchSeq_++;
  }

  // --- 0–50 und 0–100: Aufzeichnung (Live-Kurve) ab jedem Losrollen, gültig erst mit erkanntem Sprint
  if ((state_ == State::Ready || state_ == State::Done) && wasStanding && !standing) {
    state_ = State::Waiting;
    startMs_ = prevT_;  // Startzeit = letzte Messung im Stand (A10: Tempo verlässt 0)
    vTop_ = v;
    t50_ = NAN;
    cur_.clear();
    cur_.add(0, 0);
  }
  if (state_ == State::Waiting || state_ == State::Running) {
    if (v > vTop_) vTop_ = v;
    cur_.add((t - startMs_) / 1000.0f, v > 100 ? 100 : v);
    if (std::isnan(t50_) && v >= 50) t50_ = (cross(50) - startMs_) / 1000.0f;
    // Sprint nur innerhalb der ersten 3 s nach dem Anfahren; danach bleibt es eine Live-Kurve
    if (state_ == State::Waiting && launch && t - startMs_ <= cfg::SPRINT_ARM_AFTER_LEAVE_MS + 200) state_ = State::Running;
    const bool drop = v < vTop_ - cfg::SPRINT_ABORT_DROP_KMH, slow = t - startMs_ > cfg::SPRINT_MAX_MS,
               weakNow = weak && t - startMs_ >= cfg::SPRINT_ACCEL_WINDOW_MS;
    const bool over = drop || slow || weakNow;
    if (over) abortReason_ = drop ? 1 : (slow ? 2 : 3);
    if (state_ == State::Running) {
      if (!std::isnan(t50_) && std::isnan(last50Run_)) {
        last50Run_ = t50_;
        last50_ = t50_;
        result(Kind::S50, t50_, best_.s50);
      }
      if (v >= 100) {
        last100_ = (cross(100) - startMs_) / 1000.0f;
        trace_ = cur_;
        state_ = State::Done;
        doneAt_ = t ? t : 1;
        const float before = best_.s100;
        result(Kind::S100, last100_, best_.s100);
        if (std::isnan(before) || last100_ <= before) best_.trace100 = cur_;
      } else if (over) {
        abort();
      }
    } else if (state_ == State::Waiting && (over || v >= 100)) {
      abort();  // normales Anfahren: Live-Kurve endet, zählt nicht
    }
    if (state_ != State::Running) last50Run_ = NAN;
  }

  // --- 80–120: beim Durchfahren von 80 km/h mit Gaspedal ≥ 80 % oder kräftiger Beschleunigung
  if (!run80_) {
    if (!std::isnan(prevV_) && prevV_ < 80 && v >= 80 &&
        (pedalHigh || (!std::isnan(accel) && accel >= cfg::SPRINT_80_ACCEL_MS2))) {
      run80_ = true;
      start80_ = cross(80);
      vTop80_ = v;
    }
  } else {
    if (v > vTop80_) vTop80_ = v;
    if (v >= 120) {
      last80120_ = (cross(120) - start80_) / 1000.0f;
      run80_ = false;
      result(Kind::S80120, last80120_, best_.s80120);
    } else if (v < vTop80_ - cfg::SPRINT_ABORT_DROP_KMH || t - start80_ > cfg::SPRINT_MAX_MS ||
               (weak && t - start80_ >= cfg::SPRINT_ACCEL_WINDOW_MS)) {
      run80_ = false;
    }
  }

  prevT_ = t;
  prevV_ = v;
}

AutoSprint::Action AutoSprint::update(uint32_t now, bool enabled, uint16_t launchSeq, State st, uint32_t doneAtMs,
                                      float pedalPct, bool overlayOpen) {
  if (!init_) {
    init_ = true;
    seenLaunch_ = launchSeq;
    return Action::None;
  }
  if (launchSeq != seenLaunch_) {
    seenLaunch_ = launchSeq;
    // Ist ein Fenster offen, wird nicht gewechselt (A10)
    if (enabled && !overlayOpen && !switched_) {
      switched_ = true;
      switchedAt_ = now ? now : 1;
      lowSince_ = 0;
      return Action::ShowSprint;
    }
  }
  if (!switched_) return Action::None;
  if (overlayOpen) {  // Fenster geöffnet: kein Rücksprung mehr
    switched_ = false;
    return Action::None;
  }
  const bool running = st == State::Waiting || st == State::Running;
  const bool doneNow = st == State::Done && doneAtMs >= switchedAt_;
  // Gas weg zählt nur, solange keine Messung läuft: das Rohpedal liegt bei manchen Autos auch bei Vollgas unter 50 %
  lowSince_ = (!running && !std::isnan(pedalPct) && pedalPct < cfg::SPRINT_ABORT_PEDAL_PCT) ? (lowSince_ ? lowSince_ : now) : 0;
  bool ret = false;
  if (doneNow)
    ret = now - doneAtMs >= cfg::AUTO_SPRINT_RETURN_DONE_MS;  // Ergebnis 4 s stehen lassen
  else if (lowSince_ && now - lowSince_ >= cfg::AUTO_SPRINT_RETURN_LOW_MS)
    ret = true;
  else if (!running && now - switchedAt_ >= cfg::AUTO_SPRINT_NOT_RUNNING_MS)
    ret = true;
  if (now - switchedAt_ >= cfg::AUTO_SPRINT_MAX_MS) ret = true;
  if (!ret) return Action::None;
  switched_ = false;
  return Action::Return;
}

}  // namespace perf
