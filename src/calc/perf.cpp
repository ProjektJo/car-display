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

void SprintMeter::update(uint32_t t, float v, float pedal, float rpm) {
  if (std::isnan(v)) return;
  const bool standing = v < cfg::SPRINT_STAND_KMH;
  const bool wasStanding = !std::isnan(prevV_) && prevV_ < cfg::SPRINT_STAND_KMH;
  // Zeitpunkt, zu dem das Tempo x zwischen der vorigen und dieser Messung erreicht wurde (linear)
  auto cross = [&](float x) -> uint32_t {
    if (std::isnan(prevV_) || v <= prevV_) return t;
    const float f = (x - prevV_) / (v - prevV_);
    return prevT_ + static_cast<uint32_t>((f < 0 ? 0 : (f > 1 ? 1 : f)) * (t - prevT_));
  };

  // --- Sprint aus dem Stand erkennen: ≥ 1 s gestanden, dann spätestens 3 s nach dem Anfahren
  //     Gaspedal ≥ 85 % und Drehzahl ≥ 3000 U/min (A10)
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
  const bool launch = armed && !std::isnan(pedal) && pedal >= cfg::SPRINT_PEDAL_PCT && !std::isnan(rpm) &&
                      rpm >= cfg::SPRINT_MIN_RPM;
  if (launch && !launchLatched_) {
    launchLatched_ = true;
    launchSeq_++;
  }
  // Gaspedal unter 50 %: Abbruch erst nach 1,5 s (ein Schaltvorgang ist kürzer)
  const bool low = std::isnan(pedal) || pedal < cfg::SPRINT_ABORT_PEDAL_PCT;

  // --- 0–50 und 0–100: Aufzeichnung ab dem Losrollen, gültig erst mit erkanntem Sprint
  if ((state_ == State::Ready || state_ == State::Done) && wasStanding && !standing) {
    state_ = State::Waiting;
    startMs_ = prevT_;  // Startzeit = letzte Messung im Stand (A10: Tempo verlässt 0)
    vTop_ = v;
    lowSince_ = 0;
    t50_ = NAN;
    cur_.clear();
    cur_.add(0, 0);
  }
  if (state_ == State::Waiting || state_ == State::Running) {
    if (v > vTop_) vTop_ = v;
    cur_.add((t - startMs_) / 1000.0f, v > 100 ? 100 : v);
    lowSince_ = low ? (lowSince_ ? lowSince_ : t) : 0;
    if (std::isnan(t50_) && v >= 50) t50_ = (cross(50) - startMs_) / 1000.0f;
    if (state_ == State::Waiting) {
      if (launch) {
        state_ = State::Running;
      } else if (t - startMs_ > cfg::SPRINT_ARM_AFTER_LEAVE_MS) {
        abort();  // normales Anfahren: still verworfen
      }
    }
    if (state_ == State::Running) {
      if (!std::isnan(t50_) && std::isnan(last50Run_)) {
        last50Run_ = t50_;
        last50_ = t50_;
        if (std::isnan(best_.s50) || t50_ < best_.s50) {
          best_.s50 = t50_;
          bestChanged_ = true;
        }
      }
      if (v >= 100) {
        last100_ = (cross(100) - startMs_) / 1000.0f;
        trace_ = cur_;
        state_ = State::Done;
        doneAt_ = t ? t : 1;
        if (std::isnan(best_.s100) || last100_ <= best_.s100) {
          best_.s100 = last100_;
          best_.trace100 = cur_;
          bestChanged_ = true;
        }
      } else if ((lowSince_ && t - lowSince_ > cfg::SPRINT_ABORT_LOW_MS) || v < vTop_ - cfg::SPRINT_ABORT_DROP_KMH) {
        abort();
      }
    }
    if (state_ != State::Running) last50Run_ = NAN;
  }

  // --- 80–120: beim Durchfahren von 80 km/h mit Gaspedal ≥ 85 %, gleiche Abbruchregel
  if (!run80_) {
    if (!std::isnan(prevV_) && prevV_ < 80 && v >= 80 && !low && pedal >= cfg::SPRINT_PEDAL_PCT) {
      run80_ = true;
      start80_ = cross(80);
      vTop80_ = v;
      low80Since_ = 0;
    }
  } else {
    if (v > vTop80_) vTop80_ = v;
    low80Since_ = low ? (low80Since_ ? low80Since_ : t) : 0;
    if (v >= 120) {
      last80120_ = (cross(120) - start80_) / 1000.0f;
      run80_ = false;
      if (std::isnan(best_.s80120) || last80120_ < best_.s80120) {
        best_.s80120 = last80120_;
        bestChanged_ = true;
      }
    } else if ((low80Since_ && t - low80Since_ > cfg::SPRINT_ABORT_LOW_MS) || v < vTop80_ - cfg::SPRINT_ABORT_DROP_KMH) {
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
  lowSince_ = (!std::isnan(pedalPct) && pedalPct < cfg::SPRINT_ABORT_PEDAL_PCT) ? (lowSince_ ? lowSince_ : now) : 0;
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
