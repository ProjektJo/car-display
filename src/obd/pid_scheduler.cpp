#include "pid_scheduler.h"

#include <cstring>

#include "config.h"

namespace {
uint32_t periodOf(PidClass c) {
  switch (c) {
    case PidClass::Fast: return cfg::PID_PERIOD_FAST_MS;
    case PidClass::Medium: return cfg::PID_PERIOD_MEDIUM_MS;
    case PidClass::Slow: return cfg::PID_PERIOD_SLOW_MS;
    case PidClass::Rare: return cfg::PID_PERIOD_RARE_MS;
  }
  return cfg::PID_PERIOD_FAST_MS;
}
}  // namespace

void PidScheduler::add(uint8_t pid, PidClass cls, bool sprint) {
  if (pid != VOLTAGE && !supported(pid)) return;  // nie abfragen, was das Auto nicht kennt
  if (itemCount_ >= MAX_ITEMS) return;
  items_[itemCount_++] = Item{pid, cls, sprint, 0, true};
}

void PidScheduler::reset(const uint8_t supported[32], bool can, uint8_t maxPerRequest) {
  memcpy(sup_, supported, sizeof(sup_));
  can_ = can;
  maxPer_ = can ? (maxPerRequest < 1 ? 1 : (maxPerRequest > 8 ? 8 : maxPerRequest)) : 1;
  sprint_ = false;
  itemCount_ = 0;
  roundLen_ = roundPos_ = 0;

  // Gaspedal 0x49, sonst 0x4A bzw. 0x5A (relatives Pedal), sonst Drosselklappe 0x11 (A7 schnell, A10 Sprint)
  const uint8_t pedalPid = this->supported(0x49) ? 0x49 : this->supported(0x4A) ? 0x4A : this->supported(0x5A) ? 0x5A : 0;
  const bool hasPedal = pedalPid != 0;
  // Lambdasonde (Schmalband, Spannung): erste vorhandene von 0x14–0x1B bestätigt den Schub (fällt auf fast 0 V)
  uint8_t o2Pid = 0;
  for (uint8_t p = 0x14; p <= 0x1B && !o2Pid; p++)
    if (this->supported(p)) o2Pid = p;

  // schnell: jede Runde (A7)
  add(0x0D, PidClass::Fast, true);   // Tempo
  add(0x0C, PidClass::Fast, true);   // Drehzahl
  add(0x0B, PidClass::Fast, false);  // Saugrohrdruck
  if (hasPedal) {
    add(pedalPid, PidClass::Fast, true);
  } else {
    add(0x11, PidClass::Fast, true);
  }
  add(0x5E, PidClass::Fast, false);  // Kraftstoff l/h, falls das Auto ihn liefert (Verbrauch Quelle 1)
  if (o2Pid) add(o2Pid, PidClass::Fast, false);  // Schub-Bestätigung (9.10.2026)
  // Verbrauchsquelle 2 bzw. 3 schnell abfragen, damit der Momentanverbrauch mitkommt (7.10.2026)
  const bool fuelRate = this->supported(0x5E), maf = this->supported(0x10);
  if (!fuelRate && maf) add(0x10, PidClass::Fast, false);
  if (!fuelRate && !maf) add(0x43, PidClass::Fast, false);  // absolute Last

  // mittel: ca. 1 s
  add(0x0F, PidClass::Medium, false);  // Ansaugluft
  add(0x06, PidClass::Medium, false);  // STFT
  add(0x07, PidClass::Medium, false);  // LTFT
  add(0x03, PidClass::Medium, false);  // Kraftstoffsystem-Status (Schub)
  add(0x04, PidClass::Medium, false);  // Last
  if (fuelRate) add(0x10, PidClass::Medium, false);  // MAF, falls da (sonst schon schnell)
  add(0x44, PidClass::Medium, false);  // Soll-Lambda (A7 Konstanten)
  // ANNAHME: Mit Gaspedal reicht die Drosselklappe im mittleren Takt (Ersatz-Schuberkennung, A7).
  if (hasPedal) add(0x11, PidClass::Medium, false);

  // langsam: ca. 5 s
  add(0x05, PidClass::Slow, false);     // Kühlmittel
  add(VOLTAGE, PidClass::Slow, false);  // ATRV
  add(0x2F, PidClass::Slow, false);     // Tank, falls da

  // selten: 30 s
  add(0x01, PidClass::Rare, false);  // MIL und Zahl der Fehlercodes
}

void PidScheduler::buildRound(uint32_t nowMs) {
  roundLen_ = roundPos_ = 0;
  for (int i = 0; i < itemCount_; i++) {
    Item& it = items_[i];
    if (sprint_ && !it.sprint) continue;
    const bool due = it.cls == PidClass::Fast || it.never || nowMs - it.lastMs >= periodOf(it.cls);
    if (!due) continue;
    it.lastMs = nowMs;
    it.never = false;
    round_[roundLen_++] = static_cast<uint8_t>(i);
  }
}

ObdRequest PidScheduler::next(uint32_t nowMs) {
  ObdRequest r;
  if (roundPos_ >= roundLen_) buildRound(nowMs);
  if (roundLen_ == 0) {  // nichts unterstützt: nur die Bordspannung
    r.voltage = true;
    return r;
  }
  // ATRV geht immer allein
  if (items_[round_[roundPos_]].pid == VOLTAGE) {
    roundPos_++;
    r.voltage = true;
    return r;
  }
  while (roundPos_ < roundLen_ && r.count < maxPer_) {
    const uint8_t pid = items_[round_[roundPos_]].pid;
    if (pid == VOLTAGE) break;
    r.pids[r.count++] = pid;
    roundPos_++;
  }
  return r;
}

bool PidScheduler::schedules(uint8_t pid) const {
  PidClass c;
  return classOf(pid, c);
}

bool PidScheduler::classOf(uint8_t pid, PidClass& cls) const {
  for (int i = 0; i < itemCount_; i++) {
    if (items_[i].pid == pid) {
      cls = items_[i].cls;
      return true;
    }
  }
  return false;
}
