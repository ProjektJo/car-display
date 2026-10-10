#include "fuel_cut.h"

#include <cstdio>
#include <cstring>

#include "calc/eco.h"
#include "config.h"

namespace fuel {

namespace {
// Zahl mit Komma (deutsche Anzeige)
void num(char* buf, size_t n, float v, int decimals) {
  snprintf(buf, n, "%.*f", decimals, v);
  for (char* p = buf; *p; p++)
    if (*p == '.') *p = ',';
}
}  // namespace

void CutDetector::reset() {
  active_ = watch_ = false;
  why_ = Why::NoData;
  candSince_ = 0;
  head_ = count_ = 0;
  lastRpmT_ = 0;
  pedal_ = slip_ = o2_ = NAN;
  gearIdx_ = -1;
  o2Age_ = UINT32_MAX;
}

bool CutDetector::step(const In& x, uint32_t nowMs) {
  const Input& in = x.in;
  const bool o2Fresh = !std::isnan(x.o2V) && x.o2AgeMs <= cfg::CUT_O2_FRESH_MS;
  o2_ = x.o2V;
  o2Age_ = x.o2AgeMs;
  gearIdx_ = -1;
  slip_ = NAN;
  watch_ = false;
  auto off = [&](Why w) {
    why_ = w;
    active_ = false;
    candSince_ = 0;
    return false;
  };
  if (std::isnan(in.rpm) || std::isnan(in.speedKmh)) {
    count_ = 0;
    return off(Why::NoData);
  }

  // Drehzahl-Probe merken (eine je neuer Messung)
  if (x.rpmT != lastRpmT_) {
    lastRpmT_ = x.rpmT;
    const float k = eco::ratioK(in.speedKmh, in.rpm);
    if (!std::isnan(k) && in.speedKmh > 0) {
      head_ = (head_ + 1) % N;
      k_[head_] = k;
      t_[head_] = x.rpmT;
      if (count_ < N) count_++;
    }
  }

  // Fuß vom Gas: Gaspedal ist das sicherste Zeichen; ohne Pedal die Drosselklappe mit Toleranz je Drehzahl
  int footOff = -1;  // -1 unbekannt
  pedalIsThrottle_ = false;
  pedal_ = NAN;
  if (!std::isnan(in.pedalPct)) {
    const float closed = std::isnan(in.pedalClosedPct) ? 0.0f : in.pedalClosedPct;
    pedal_ = in.pedalPct - closed;
    footOff = in.pedalPct <= closed + cfg::CUT_PEDAL_MARGIN_PCT;
  } else if (!std::isnan(in.throttlePct)) {
    const float closed = std::isnan(x.throttleClosedPct) ? 0.0f : x.throttleClosedPct;
    const float rpmOver = in.rpm > cfg::CUT_FALLBACK_MIN_RPM ? in.rpm - cfg::CUT_FALLBACK_MIN_RPM : 0.0f;
    const float tol = cfg::CUT_THROTTLE_MARGIN_PCT + rpmOver / 1000.0f * cfg::CUT_THROTTLE_PER_1000RPM_PCT;
    pedal_ = in.throttlePct - closed;
    pedalIsThrottle_ = true;
    footOff = in.throttlePct <= closed + tol;
  }
  if (footOff == 0) return off(Why::Gas);  // Gasgeben beendet den Schub sofort, auch wenn 0x03 noch 4 zeigt

  // Das Steuergerät meldet den Schub selbst
  if (x.hasFuelSys && !std::isnan(in.fuelSys) && static_cast<int>(in.fuelSys) == cfg::FUEL_SYS_DECEL_CUT) {
    why_ = Why::FuelSys;
    active_ = true;
    candSince_ = 0;
    return true;
  }
  if (in.rpm <= cfg::CUT_FALLBACK_MIN_RPM || in.speedKmh <= cfg::CUT_FALLBACK_MIN_SPEED_KMH) return off(Why::Slow);
  watch_ = true;

  // Gang: Tempo/Drehzahl muss zu einem gelernten Gang passen (± 5 %)
  const float kNow = eco::ratioK(in.speedKmh, in.rpm);
  if (x.gearCount > 0 && x.gears) {
    gearIdx_ = eco::matchGear(x.gears, x.gearCount, kNow, cfg::CUT_GEAR_TOL);
    if (gearIdx_ < 0) return off(Why::NoGear);
    gearNum_ = eco::gearNumber(x.gears, x.gearCount, gearIdx_);
  }

  // Ausgekuppelt: k gegen die jüngste Probe, die mindestens 0,4 s alt ist. Im Gang bleibt k gleich, auch beim
  // Bremsen mit dem Motor; fällt die Drehzahl schneller als das Tempo, steigt k. Toleranz: 4 % plus eine
  // Tempo-Stufe (1,5 km/h), weil das Tempo nur in ganzen km/h kommt.
  for (int n = 1; n < count_; n++) {
    const int i = (head_ - n + N) % N;
    const uint32_t age = lastRpmT_ - t_[i];
    if (age > cfg::CUT_SLIP_MAX_AGE_MS) break;
    if (age >= cfg::CUT_SLIP_WINDOW_MS) {
      slip_ = kNow / k_[i] - 1.0f;
      break;
    }
  }
  if (!std::isnan(slip_)) {
    const float tol = cfg::CUT_SLIP_TOL + cfg::CUT_SLIP_SPEED_STEP_KMH / in.speedKmh;
    if (std::fabs(slip_) > tol) return off(Why::Declutched);
  }

  // Lambda nur als Bestätigung, wenn die Messung frisch ist
  if (o2Fresh && x.o2V > cfg::CUT_O2_MAX_V) return off(Why::Rich);
  if (footOff < 0 && !o2Fresh) return off(Why::NoPedal);

  // Einschaltverzug
  if (active_) {
    why_ = Why::Cut;
    return true;
  }
  if (!candSince_) candSince_ = nowMs ? nowMs : 1;
  if (nowMs - candSince_ >= cfg::CUT_ON_DELAY_MS) {
    active_ = true;
    why_ = Why::Cut;
    return true;
  }
  why_ = Why::Waiting;
  return false;
}

void CutDetector::describe(char* buf, size_t n) const {
  if (!n) return;
  char a[12], b[12];
  auto details = [&](const char* head) {
    size_t len = snprintf(buf, n, "%s", head);
    auto add = [&](const char* fmt, const char* s1, const char* s2) {
      if (len < n) len += snprintf(buf + len, n - len, fmt, s1, s2);
    };
    if (!std::isnan(pedal_)) {
      num(a, sizeof(a), pedal_ < 0 ? 0.0f : pedal_, 0);
      add("%s %s %%", pedalIsThrottle_ ? "Klappe" : "Pedal", a);
    } else {
      add("%s%s", "ohne Pedal", "");
    }
    if (gearIdx_ >= 0) {
      snprintf(a, sizeof(a), "%d", gearNum_);
      add(", %s%s", "Gang ", a);
    } else {
      add(", %s%s", "Gang ungelernt", "");
    }
    if (std::isnan(o2_)) {
      add(", %s%s", "ohne Lambda", "");
    } else {
      num(a, sizeof(a), o2_, 2);
      num(b, sizeof(b), o2Age_ > 99000 ? 99.0f : o2Age_ / 1000.0f, 1);
      add(o2Age_ <= cfg::CUT_O2_FRESH_MS ? ", Lambda %s V %s s" : ", Lambda alt (%s V %s s)", a, b);
    }
  };
  switch (why_) {
    case Why::NoData: snprintf(buf, n, "\xE2\x80\x93"); break;
    case Why::FuelSys: snprintf(buf, n, "ja: 0x03 meldet Schub"); break;
    case Why::Slow: snprintf(buf, n, "nein: unter 1200 U/min oder 15 km/h"); break;
    case Why::Gas:
      num(a, sizeof(a), pedal_, 0);
      snprintf(buf, n, "nein: Gas (%s %s %%)", pedalIsThrottle_ ? "Klappe" : "Pedal", a);
      break;
    case Why::NoPedal: snprintf(buf, n, "nein: kein Pedal, Lambda alt"); break;
    case Why::NoGear: snprintf(buf, n, "nein: passt zu keinem Gang"); break;
    case Why::Declutched:
      num(a, sizeof(a), std::fabs(slip_) * 100.0f, 0);
      snprintf(buf, n, "nein: ausgekuppelt (Drehzahl %s %% daneben)", a);
      break;
    case Why::Rich:
      num(a, sizeof(a), o2_, 2);
      num(b, sizeof(b), o2Age_ / 1000.0f, 1);
      snprintf(buf, n, "nein: Lambda %s V (%s s)", a, b);
      break;
    case Why::Waiting: details("gleich: "); break;
    case Why::Cut: details("ja: "); break;
  }
}

}  // namespace fuel
