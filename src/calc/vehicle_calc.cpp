#include "vehicle_calc.h"

#include <algorithm>
#include <cstring>
#include <initializer_list>

#include "config.h"

void initPersist(PersistState& st, uint8_t profileId) {
  memset(&st, 0, sizeof(st));
  st.magic = PERSIST_MAGIC;
  st.version = PERSIST_VERSION;
  st.profileId = profileId;
  st.ring1.init(cfg::AVG1_SLOTS, cfg::AVG1_SLOT_M);
  st.ring10.init(cfg::AVG10_SLOTS, cfg::AVG10_SLOT_M);
  st.ring100.init(cfg::AVG100_SLOTS, cfg::AVG100_SLOT_M);
  st.prevFillL100 = NAN;
  st.fills.clear();
  st.mixPrice = NAN;
  st.pumpPrice = NAN;
  st.levelAtStopPct = NAN;
  st.coolantLastC = NAN;
  st.trip.maxCoolantC = NAN;
  st.trip.ecoScore = NAN;
  st.gearHist.clear();
  st.idleLph = NAN;
  st.hasLastTrip = 0;
  st.sprintBest.clear();
  st.autoGoal = NAN;
  st.odoOffsetKm = NAN;
  st.oilDueKm = st.inspDueKm = NAN;
  st.oilIntervalKm = cfg::OIL_INTERVAL_DEFAULT_KM;
  st.inspIntervalKm = cfg::INSP_INTERVAL_DEFAULT_KM;
}

float autoGoalFrom(float avg) {
  // (siehe vehicle_calc.h)
  if (std::isnan(avg)) return NAN;
  const float g = avg - (avg > cfg::GOAL_AUTO_THRESHOLD ? cfg::GOAL_AUTO_MINUS_HIGH : cfg::GOAL_AUTO_MINUS_LOW);
  return g < cfg::GOAL_MIN ? cfg::GOAL_MIN : g;
}

void VehicleCalc::load(const Profile& p, const PersistState* saved, uint32_t nowMs) {
  profile_ = p;
  if (saved && saved->magic == PERSIST_MAGIC && saved->version == PERSIST_VERSION && saved->profileId == p.id) {
    st_ = *saved;
    if (st_.thermoBlockStarts > 0) st_.thermoBlockStarts--;  // Thermostat: höchstens alle 10 Starts (A9)
  } else {
    initPersist(st_, p.id);
  }
  active_ = true;
  out_ = Outputs{};
  tripDecided_ = false;
  coolantAtStopC_ = st_.coolantLastC;  // letzter gespeicherter Wert = "beim Abstellen" (A8)
  hasTripRecord_ = false;
  profileChanged_ = false;
  throttleClosed_ = NAN;
  engineOffSinceMs_ = 0;
  standstillSinceMs_ = 0;
  standstillSaved_ = false;
  saveNow_ = false;
  lastSaveMs_ = nowMs;  // erst eine Minute nach dem Laden wieder regulär speichern
  kmAtSave_ = st_.totalKm;
  litersAtSave_ = st_.totalL;
  levelSmooth_ = NAN;
  for (int i = 0; i < WIN; i++) winDt_[i] = 0;
  pedalClosed_ = NAN;
  pedalMax_ = NAN;
  pedalSmooth_ = NAN;
  speedT_ = 0;
  speedPrev_ = NAN;
  accel_ = NAN;
  stable_.reset();
  check_.reset();
  learnPaused_ = false;
  lastGearSearchMs_ = nowMs;
  engineWasOn_ = false;
  detectDone_ = true;
  thermoDriveS_ = thermoFastS_ = 0;
  iatStart_ = NAN;
  gpsEpochAtStop_ = st_.lastGpsEpoch;
  thermoFired_ = false;
  sprint_.reset();
  sprint_.setBest(st_.sprintBest);
}

// Beim Start entscheiden, ob die gespeicherte Fahrt weiterläuft (A8 Fahrt-Ende ohne Uhr).
// ANNAHME: Ohne Kühlmitteltemperatur (0x05 nicht unterstützt) und ohne GPS beginnt immer eine neue Fahrt.
void VehicleCalc::decideTrip(const CarState& s, uint32_t nowMs) {
  // Mit GPS-Uhrzeit: Pause über 5 min = neue Fahrt (A8). ANNAHME: nur wenn die Uhrzeit schon beim
  // Start bekannt ist; sonst entscheidet die Kühlmitteltemperatur.
  if (s.gpsTimeValid && s.gpsEpoch && gpsEpochAtStop_) {
    tripDecided_ = true;
    if (st_.trip.active && s.gpsEpoch >= gpsEpochAtStop_ && s.gpsEpoch - gpsEpochAtStop_ <= cfg::TRIP_GPS_PAUSE_S) return;
    finishTrip();
    trip::start(st_.trip, ++st_.lastTripNumber);
    return;
  }
  const float coolant = s.coolant.get(nowMs);
  const bool coolantMissing = s.link.supportedKnown && !s.link.pidSupported(0x05);
  if (std::isnan(coolant) && !coolantMissing) return;  // warten, bis der erste Wert da ist
  tripDecided_ = true;
  if (st_.trip.active && trip::continues(coolantAtStopC_, coolant)) return;
  finishTrip();
  trip::start(st_.trip, ++st_.lastTripNumber);
}

void VehicleCalc::finishTrip() {
  if (st_.trip.active && st_.trip.km >= cfg::TRIP_MIN_RECORD_KM) {
    tripRecord_ = trip::toRecord(st_.trip, profile_.id);
    tripRecord_.date = gpsDate_;  // mit GPS das Datum, sonst 0
    hasTripRecord_ = true;
    st_.lastTrip = tripRecord_;  // Start-Karte beim nächsten Einschalten
    st_.hasLastTrip = 1;
  }
  st_.trip.active = 0;
  saveNow_ = true;
}

void VehicleCalc::endTrip() {
  if (!active_) return;
  finishTrip();
}

// Automatische Tankerkennung mit 0x2F (A7): beim Motorstart 10 s im Stand mitteln und mit dem Füllstand
// vom letzten Abstellen vergleichen. Während der Motor läuft, merkt sich die Firmware den geglätteten Füllstand.
void VehicleCalc::stepRefuelDetect(const CarState& s, uint32_t nowMs, bool engineOn) {
  const bool has2F = s.link.pidSupported(0x2F);
  const float level = s.fuelLevel.get(nowMs);
  const float speed = s.speed.get(nowMs);
  if (engineOn && !engineWasOn_) {  // Motor springt an
    detectSince_ = nowMs ? nowMs : 1;
    detectDone_ = !has2F || std::isnan(st_.levelAtStopPct);
    detectSum_ = 0;
    detectN_ = 0;
  }
  engineWasOn_ = engineOn;
  if (!engineOn || !has2F) return;
  if (!detectDone_) {
    // nur im Stand mitteln (die Anzeige schwappt beim Fahren); losgefahren = mit dem bisherigen Mittel entscheiden
    const bool standing = !std::isnan(speed) && speed < 1.0f;
    if (standing && !std::isnan(level)) {
      detectSum_ += level;
      detectN_++;
    }
    if (nowMs - detectSince_ >= cfg::REFUEL_AVG_MS || !standing) {
      detectDone_ = true;
      if (detectN_ > 0) {
        const float avgLevel = static_cast<float>(detectSum_ / detectN_);
        const float rise = avgLevel - st_.levelAtStopPct;
        if (rise >= cfg::REFUEL_MIN_RISE_PCT) {
          float liters = rise / 100.0f * profile_.tankL;
          // fast voll: berechnete Liter seit der letzten Vollbetankung sind genauer (A7)
          if (avgLevel > cfg::REFUEL_FULL_PCT && st_.hadFullFill && st_.calComputedL > 0)
            liters = static_cast<float>(st_.calComputedL);
          out_.refuelL = liters;
          out_.refuelSeq++;
        }
        st_.levelAtStopPct = avgLevel;
        levelSmooth_ = avgLevel;
      }
    }
    return;
  }
  // Beim Fahren den geglätteten Füllstand als "beim Abstellen" merken
  if (!std::isnan(levelSmooth_)) st_.levelAtStopPct = levelSmooth_;
}

void VehicleCalc::setGoal(uint8_t mode, float fixL100) {
  goalMode_ = mode;
  goalFix_ = fixL100;
}

void VehicleCalc::setBody(uint8_t body) {
  if (!active_ || body >= cfg::BODY_TYPE_COUNT || body == profile_.body) return;
  profile_.body = body;
  profileChanged_ = true;
}

void VehicleCalc::setColdRpm(uint16_t rpm) {
  if (!active_ || rpm == profile_.coldRpmLimit) return;
  profile_.coldRpmLimit = rpm;
  profileChanged_ = true;
}

void VehicleCalc::setOdo(float km) {
  if (!active_ || !(km >= 0)) return;
  st_.odoOffsetKm = static_cast<float>(km - st_.totalKm);
  // Beim ersten Eintrag beginnen beide Zähler hier
  if (std::isnan(st_.oilDueKm)) st_.oilDueKm = km + st_.oilIntervalKm;
  if (std::isnan(st_.inspDueKm)) st_.inspDueKm = km + st_.inspIntervalKm;
  saveNow_ = true;
}

void VehicleCalc::maintenanceDone(int which) {
  if (!active_ || std::isnan(st_.odoOffsetKm)) return;
  const float odo = static_cast<float>(st_.totalKm + st_.odoOffsetKm);
  if (which == 0) st_.oilDueKm = odo + st_.oilIntervalKm;
  else st_.inspDueKm = odo + st_.inspIntervalKm;
  saveNow_ = true;
}

void VehicleCalc::setInterval(int which, float km) {
  if (!active_ || !(km > 0)) return;
  float& interval = which == 0 ? st_.oilIntervalKm : st_.inspIntervalKm;
  float& due = which == 0 ? st_.oilDueKm : st_.inspDueKm;
  if (!std::isnan(due)) due += km - interval;  // gleicher letzter Termin, neues Intervall
  interval = km;
  saveNow_ = true;
}

void VehicleCalc::resetAverages(uint8_t mask) {
  if (!active_) return;
  if (mask & 1) st_.ring1.init(cfg::AVG1_SLOTS, cfg::AVG1_SLOT_M);
  if (mask & 2) st_.ring10.init(cfg::AVG10_SLOTS, cfg::AVG10_SLOT_M);
  if (mask & 4) st_.ring100.init(cfg::AVG100_SLOTS, cfg::AVG100_SLOT_M);
  if (mask & 8) {
    st_.fillKm = st_.fillL = 0;
    st_.prevFillL100 = NAN;
  }
  saveNow_ = true;
}

// Thermostat-Check (A9, Z 10): Motor bleibt trotz langer Fahrt kalt
void VehicleCalc::stepThermo(const CarState& s, uint32_t nowMs, float dtS, bool engineOn) {
  const float coolant = s.coolant.get(nowMs);
  // Wird der Motor wieder normal warm, verschwindet der Eintrag
  if (st_.thermoActive && !std::isnan(coolant) && coolant >= cfg::THERMO_COOLANT_C) {
    st_.thermoActive = 0;
    saveNow_ = true;
  }
  if (!engineOn) return;
  if (std::isnan(iatStart_)) iatStart_ = s.iat.get(nowMs);
  thermoDriveS_ += dtS;
  const float speed = s.speed.get(nowMs);
  if (!std::isnan(speed) && speed > cfg::THERMO_FAST_KMH) thermoFastS_ += dtS;
  if (thermoFired_ || st_.thermoBlockStarts > 0) return;
  if (thermoDriveS_ > cfg::THERMO_DRIVE_S && thermoFastS_ > cfg::THERMO_FAST_S && !std::isnan(coolant) &&
      coolant < cfg::THERMO_COOLANT_C && !std::isnan(iatStart_) && iatStart_ > cfg::THERMO_MIN_IAT_C) {
    thermoFired_ = true;
    st_.thermoActive = 1;
    st_.thermoBlockStarts = cfg::THERMO_EVERY_STARTS;
    out_.thermoSeq++;
    saveNow_ = true;
  }
}

void VehicleCalc::setKmFactor(float f) {
  if (!active_ || !(f >= cfg::KMF_MIN && f <= cfg::KMF_MAX)) return;
  // halbe Korrektur je Vergleich, dämpft Ausreißer
  const float next = (profile_.kmFactor + f) / 2.0f;
  if (std::fabs(next - profile_.kmFactor) < 0.001f) return;
  profile_.kmFactor = next;
  profileChanged_ = true;
}

void VehicleCalc::relearnGears() {
  if (!active_) return;
  st_.gearHist.clear();
  profile_.gearCount = 0;
  for (float& g : profile_.gears) g = 0;
  profileChanged_ = true;
  learnPaused_ = false;
  out_.gearMismatch = false;
  check_.reset();
  saveNow_ = true;
}

void VehicleCalc::dismissGearCheck() {
  learnPaused_ = false;
  out_.gearMismatch = false;
}

// Gänge aus dem Histogramm ins Profil übernehmen (A6). Ein Datenblatt-Eintrag mit mehr Gängen bleibt,
// bis das Lernen mindestens genauso viele Gänge gefunden hat.
void VehicleCalc::updateGears(uint32_t nowMs) {
  if (learnPaused_ || nowMs - lastGearSearchMs_ < cfg::GEAR_RECHECK_MS) return;
  lastGearSearchMs_ = nowMs;
  if (st_.gearHist.total < cfg::GEAR_LEARN_MIN_TOTAL) return;
  float found[MAX_GEARS];
  const int n = eco::findGears(st_.gearHist, found, MAX_GEARS);
  if (n < 2 || n < profile_.gearCount) return;
  bool same = n == profile_.gearCount;
  for (int i = 0; same && i < n; i++) same = std::fabs(found[i] / profile_.gears[i] - 1.0f) < 0.02f;
  if (same) return;
  for (int i = 0; i < MAX_GEARS; i++) profile_.gears[i] = i < n ? found[i] : 0.0f;
  profile_.gearCount = static_cast<uint8_t>(n);
  profileChanged_ = true;
}

// Gang, Schaltempfehlung, Lernen, Fahrzeug-Prüfung, Eco-Score, Bremsenergie, Schub gespart (A6, A9)
void VehicleCalc::stepEco(const CarState& s, uint32_t nowMs, float dtS, bool engineOn, bool cut, float lph, float dkm,
                          float dl) {
  const float speed = s.speed.get(nowMs);
  const float rpm = s.rpm.get(nowMs);

  // Gaspedal, ersatzweise Drosselklappe (A7); "geschlossen" relativ zum kleinsten Wert
  float pedal = s.pedal.get(nowMs);
  if (std::isnan(pedal)) pedal = s.throttle.get(nowMs);
  if (engineOn && !std::isnan(pedal) && (std::isnan(pedalClosed_) || pedal < pedalClosed_)) pedalClosed_ = pedal;
  const bool pedalClosed = std::isnan(pedal) || (!std::isnan(pedalClosed_) && pedal <= pedalClosed_ + cfg::PEDAL_CLOSED_MARGIN_PCT);
  out_.pedalPct = pedal;
  // Für den Sprint: Pedal relativ zum gelernten Bereich (leer … Vollgas). Viele Autos melden bei Vollgas
  // nur 70–80 %; bis ein höherer Wert gesehen wurde, gilt "leer + 55 %" als Vollgas.
  if (!std::isnan(pedal) && (std::isnan(pedalMax_) || pedal > pedalMax_)) pedalMax_ = pedal;
  float pedalRel = NAN;
  if (!std::isnan(pedal) && !std::isnan(pedalClosed_)) {
    const float full = std::max(pedalMax_, pedalClosed_ + cfg::SPRINT_PEDAL_SPAN_MIN);
    pedalRel = (pedal - pedalClosed_) / (full - pedalClosed_) * 100.0f;
  }

  // Beschleunigung aus zwei Tempo-Messungen, gefiltert
  if (std::isnan(speed)) {
    accel_ = NAN;
    speedPrev_ = NAN;
  } else if (s.speed.t != speedT_) {
    const float dtSample = (s.speed.t - speedT_) / 1000.0f;
    if (!std::isnan(speedPrev_) && dtSample > 0 && dtSample < 2.0f) {
      const float a = (speed - speedPrev_) / 3.6f / dtSample;
      const float k = dtSample / (cfg::ACCEL_SMOOTH_TAU_S + dtSample);
      accel_ = std::isnan(accel_) ? a : accel_ + k * (a - accel_);
    }
    speedPrev_ = speed;
    speedT_ = s.speed.t;
    // Sprintmessung mit jeder neuen Tempo-Messung (A10)
    sprint_.update(s.speed.t, speed, pedalRel, accel_);
    if (sprint_.takeBestChanged()) {
      st_.sprintBest = sprint_.best();
      saveNow_ = true;
    }
  }
  out_.accelMs2 = accel_;
  // Mit MPU6050: gemessene Beschleunigung und Hubarbeit am Berg (A10); m·a + m·g·sin(Steigung)
  const float slope = s.slopePct.get(nowMs);
  const float imuLong = s.imuReady ? s.imuLong.get(nowMs) : NAN;
  const float aReal = std::isnan(imuLong) ? accel_ : imuLong;
  const float aEff = std::isnan(aReal) ? NAN : aReal + (std::isnan(slope) ? 0.0f : cfg::GRAVITY * std::sin(std::atan(slope / 100.0f)));
  // Geschätzte Leistung am Rad (A10): (m·a + Luft + Rollen [+ Hang]) · v
  {
    const cfg::BodyType& b = profile_.bodyType();
    const float p = std::isnan(speed) ? NAN : eco::wheelPowerW(b.massKg, b.cwA, speed / 3.6f, aEff);
    out_.powerKw = std::isnan(p) ? NAN : (p > 0 ? p / 1000.0f : 0.0f);
  }

  // Gang
  int idx = -1;
  out_.gear = eco::displayGear(profile_.gears, profile_.gearCount, speed, rpm, idx);

  // Lernen aus stabilen Phasen (A6) und Fahrzeug-Prüfung
  const float k = eco::ratioK(speed, rpm);
  const bool learnable = engineOn && !std::isnan(k) && speed > cfg::GEAR_LEARN_MIN_KMH && rpm > cfg::GEAR_LEARN_MIN_RPM &&
                         !pedalClosed && !cut;
  if (learnable && stable_.add(k, nowMs)) {
    if (!learnPaused_) st_.gearHist.add(k);
    if (profile_.gearCount > 0 && !checkAsked_) {
      check_.add(eco::matchGear(profile_.gears, profile_.gearCount, k) >= 0, dtS);
      if (check_.mismatch()) {
        checkAsked_ = true;
        learnPaused_ = true;
        out_.gearMismatch = true;
      }
    }
  } else if (!learnable) {
    stable_.reset();
  }
  updateGears(nowMs);

  // Spartempo (Z 1): höchster gelernter Gang, Tempo ± 3 km/h und Pedal ruhig seit 20 s
  {
    const bool top = profile_.gearCount > 0 && idx == profile_.gearCount - 1;
    const bool level = std::isnan(slope) || std::fabs(slope) < cfg::TEMPO_MAX_SLOPE_PCT;  // mit MPU: eben (Z 1)
    if (engineOn && top && level && !cut && !std::isnan(speed) && !std::isnan(pedal)) {
      if (!tempoSince_ || std::fabs(speed - tempoRefV_) > cfg::TEMPO_BAND_KMH ||
          std::fabs(pedal - tempoRefP_) > cfg::TEMPO_PEDAL_BAND_PCT) {
        tempoSince_ = nowMs ? nowMs : 1;
        tempoRefV_ = speed;
        tempoRefP_ = pedal;
      } else if (nowMs - tempoSince_ >= cfg::TEMPO_STEADY_MS) {
        const int cls = static_cast<int>(std::lround((tempoRefV_ - cfg::TEMPO_FIRST_KMH) / cfg::TEMPO_STEP_KMH));
        if (cls >= 0 && cls < cfg::TEMPO_CLASSES) {
          st_.tempoKm[cls] += dkm;
          st_.tempoL[cls] += dl;
        }
      }
    } else {
      tempoSince_ = 0;
    }
  }

  // Schaltempfehlung (A9)
  const float nextMin = profile_.fuel == FuelType::Diesel ? cfg::SHIFT_NEXT_MIN_RPM_DIESEL : cfg::SHIFT_NEXT_MIN_RPM_PETROL;
  out_.shiftAdvice = engineOn && !std::isnan(speed) && speed >= cfg::MOVING_MIN_KMH &&
                     eco::shiftAdvice(rpm, pedal, pedalClosed, cut, profile_.gears, profile_.gearCount, idx,
                                      profile_.shiftRpm, nextMin);

  // Leerlaufverbrauch lernen: warm, im Stand, kein Schub (Schub gespart, A9)
  const float coolant = s.coolant.get(nowMs);
  if (engineOn && !cut && !std::isnan(lph) && lph > 0 && !std::isnan(speed) && speed < 1.0f && !std::isnan(coolant) &&
      coolant >= cfg::IDLE_LEARN_MIN_COOLANT_C) {
    const float a = dtS / cfg::IDLE_LEARN_TAU_S;
    st_.idleLph = std::isnan(st_.idleLph) ? lph : st_.idleLph + (a > 1 ? 1 : a) * (lph - st_.idleLph);
  }

  // Geglättetes Pedal (ruhiges Gas)
  float pedalDelta = 0;
  if (!std::isnan(pedal)) {
    if (std::isnan(pedalSmooth_)) {
      pedalSmooth_ = pedal;
    } else {
      const float a = dtS / (cfg::PEDAL_SMOOTH_TAU_S + dtS);
      const float next = pedalSmooth_ + a * (pedal - pedalSmooth_);
      pedalDelta = std::fabs(next - pedalSmooth_);
      pedalSmooth_ = next;
    }
  }

  trip::TripState& t = st_.trip;
  if (tripDecided_ && t.active && engineOn && !std::isnan(speed)) {
    if (speed >= cfg::MOVING_MIN_KMH) {
      t.moveS += dtS;
      const bool sailing = out_.gear == eco::GEAR_NEUTRAL && !std::isnan(accel_) && accel_ > -cfg::ROLL_MAX_DECEL_MS2;
      if (cut || sailing) t.rollS += dtS;
      if (speed > t.vMax) t.vMax = speed;
      if (!pedalClosed && !std::isnan(out_.powerKw) && out_.powerKw > t.kwPeak) t.kwPeak = out_.powerKw;
      t.pedalAbs += pedalDelta;
      if (out_.shiftAdvice) t.shiftOpenS += dtS;
      // Bremsenergie nur ohne Gas (A9)
      if (pedalClosed) {
        const cfg::BodyType& b = profile_.bodyType();
        t.brakeJ += eco::brakePowerW(b.massKg, b.cwA, speed / 3.6f, aEff) * dtS;
      }
    }
    const float heat = profile_.fuel == FuelType::Diesel ? cfg::HEAT_DIESEL_MJ_PER_L : cfg::HEAT_PETROL_MJ_PER_L;
    t.brakedL = eco::litersFromJoule(t.brakeJ, heat);
    eco::ScoreInput si;
    si.moveS = static_cast<float>(t.moveS);
    si.rollS = static_cast<float>(t.rollS);
    si.pedalAbs = static_cast<float>(t.pedalAbs);
    si.shiftOpenS = static_cast<float>(t.shiftOpenS);
    si.brakedL = t.brakedL;
    si.km = static_cast<float>(t.km);
    si.idleS = static_cast<float>(t.idleS);
    si.driveS = static_cast<float>(t.durationS);
    t.ecoScore = eco::score(si).score;
  }
  const float idle = std::isnan(st_.idleLph) ? cfg::IDLE_LPH_DEFAULT : st_.idleLph;
  out_.ecoScore = t.active ? t.ecoScore : NAN;
  out_.brakedL = t.active ? t.brakedL : NAN;
  out_.cutSavedL = t.active ? static_cast<float>(t.cutS) * idle / 3600.0f : NAN;
  out_.tripVmax = t.active ? t.vMax : NAN;
  out_.tripKwPeak = t.active ? t.kwPeak : NAN;
}

void VehicleCalc::step(const CarState& s, uint32_t nowMs, float dtS) {
  if (!active_ || !(dtS > 0)) return;
  const LinkInfo& li = s.link;

  fuel::Input in;
  in.speedKmh = s.speed.get(nowMs);
  in.rpm = s.rpm.get(nowMs);
  in.mapKpa = s.map.get(nowMs);
  in.iatC = s.iat.get(nowMs);
  in.stftPct = s.stft.get(nowMs);
  in.ltftPct = s.ltft.get(nowMs);
  in.mafGs = s.maf.get(nowMs);
  in.fuelRateLph = s.fuelRate.get(nowMs);
  in.lambda = s.lambdaCmd.get(nowMs);
  in.fuelSys = s.fuelSys.get(nowMs);
  in.throttlePct = s.throttle.get(nowMs);

  const fuel::Source src = fuel::chooseSource(profile_.fuel, li.pidSupported(0x5E), li.pidSupported(0x10),
                                              li.pidSupported(0x0B), li.pidSupported(0x0C));
  out_.source = src;
  fuel::Engine eng;
  eng.fuel = profile_.fuel;
  eng.displacementL = profile_.displacementL;
  eng.ve = profile_.ve;
  eng.fuelCal = profile_.fuelCal;

  const bool engineOn = !std::isnan(in.rpm) && in.rpm > cfg::ENGINE_RUNNING_MIN_RPM;
  const bool engineOff = !engineOn;  // ohne Drehzahl-Meldung (Zündung aus, Adapter weg) zählt der Motor als aus

  if (engineOn && !std::isnan(in.throttlePct) && (std::isnan(throttleClosed_) || in.throttlePct < throttleClosed_))
    throttleClosed_ = in.throttlePct;

  // Verbrauch dieses Schritts: Schub = exakt 0 (A7), Motor aus = 0, sonst aus der Quelle
  bool cut = false;
  float lph = NAN;
  if (engineOn) {
    cut = fuel::isFuelCut(li.pidSupported(0x03), in, throttleClosed_);
    lph = cut ? 0.0f : fuel::rateLph(src, eng, in);
  } else if (!std::isnan(in.rpm)) {
    lph = 0.0f;
  }
  out_.fuelCut = cut;

  const float dm = std::isnan(in.speedKmh) ? 0.0f : in.speedKmh / 3.6f * dtS * profile_.kmFactor;  // Meter
  const float dml = std::isnan(lph) ? 0.0f : lph / 3.6f * dtS;                                      // Milliliter
  const float dkm = dm / 1000.0f, dl = dml / 1000.0f;

  // Mittelwerte, Summen, Kalibrierzeitraum, Tankmodell. Ohne bekannten Verbrauch (Quelle fehlt oder
  // Werte veraltet) zählt auch die Strecke dort nicht, sonst fielen die Schnitte zu niedrig aus.
  const bool fuelKnown = !std::isnan(lph);
  if (fuelKnown && (dm > 0 || dml > 0)) {
    st_.ring1.add(dm, dml);
    st_.ring10.add(dm, dml);
    st_.ring100.add(dm, dml);
    st_.totalKm += dkm;
    st_.totalL += dl;
    st_.fillKm += dkm;
    st_.fillL += dl;
    if (st_.hadFullFill) {
      st_.calKm += dkm;
      st_.calComputedL += dl;
    }
    if (st_.tankModelValid) {
      st_.tankModelL -= dl;
      if (st_.tankModelL < 0) st_.tankModelL = 0;
    }
  }

  // Kühlmittel merken (beim nächsten Start der Wert "beim Abstellen")
  const float coolant = s.coolant.get(nowMs);
  if (!std::isnan(coolant)) st_.coolantLastC = coolant;
  stepRefuelDetect(s, nowMs, engineOn);
  stepThermo(s, nowMs, dtS, engineOn);
  if (s.gpsTimeValid && s.gpsEpoch) {
    st_.lastGpsEpoch = s.gpsEpoch;
    gpsDate_ = s.gpsDate;
  }

  // Fahrt
  if (!tripDecided_) decideTrip(s, nowMs);
  if (tripDecided_) {
    if (!st_.trip.active && engineOn) trip::start(st_.trip, ++st_.lastTripNumber);
    if (st_.trip.active) {
      trip::TripState& t = st_.trip;
      // Strecke ohne Verbrauch nur, wenn es gar keine Quelle gibt (dann zeigt Ø Fahrt "–"); kurze
      // Lücken lässt die Fahrt aus wie die Schnitte
      if (fuelKnown || src == fuel::Source::None) t.km += dkm;
      t.liters += dl;
      if (engineOn) {
        t.durationS += dtS;
        if (!std::isnan(in.speedKmh) && in.speedKmh < 1.0f) {
          t.idleS += dtS;
          t.idleL += dl;
        }
        if (cut) t.cutS += dtS;
        if (in.rpm > t.maxRpm) t.maxRpm = in.rpm;
      }
      if (!std::isnan(coolant) && (std::isnan(t.maxCoolantC) || coolant > t.maxCoolantC)) t.maxCoolantC = coolant;
      if (dl > 0 && !std::isnan(st_.mixPrice)) {
        t.cost += dl * st_.mixPrice;
        t.costKnown = 1;
      }
    }
  }
  // Strom bleibt bei Zündung aus an: 5 min ohne Motor beendet die Fahrt (A8)
  if (engineOn) {
    engineOffSinceMs_ = 0;
  } else {
    if (engineOffSinceMs_ == 0) engineOffSinceMs_ = nowMs ? nowMs : 1;
    if (tripDecided_ && st_.trip.active && nowMs - engineOffSinceMs_ >= cfg::TRIP_ENGINE_OFF_END_MS) finishTrip();
  }

  // Stillstand über 10 s: einmal speichern (A8)
  const bool standing = engineOff || (!std::isnan(in.speedKmh) && in.speedKmh < 1.0f);
  if (standing) {
    if (standstillSinceMs_ == 0) standstillSinceMs_ = nowMs ? nowMs : 1;
  } else {
    standstillSinceMs_ = 0;
    standstillSaved_ = false;
  }

  stepEco(s, nowMs, dtS, engineOn, cut, lph, fuelKnown ? dkm : 0.0f, fuelKnown ? dl : 0.0f);

  // Momentanverbrauch: 1-s-Fenster
  if (std::isnan(lph)) {
    for (int i = 0; i < WIN; i++) winDt_[i] = 0;  // Lücke: ältere Werte gelten nicht mehr als "momentan"
  } else {
    winHead_ = (winHead_ + 1) % WIN;
    winDt_[winHead_] = dtS;
    winLph_[winHead_] = lph * dtS;
    winSpeed_[winHead_] = std::isnan(in.speedKmh) ? NAN : in.speedKmh * dtS;
  }
  updateOutputs(s, nowMs, dtS);
}

void VehicleCalc::updateOutputs(const CarState& s, uint32_t nowMs, float dtS) {
  // 1-s-Mittel aus den jüngsten Schritten
  float t = 0, l = 0, v = 0;
  bool speedOk = true;
  for (int n = 0; n < WIN && t < cfg::INSTANT_WINDOW_MS / 1000.0f; n++) {
    const int i = (winHead_ - n + WIN) % WIN;
    if (winDt_[i] <= 0) break;
    t += winDt_[i];
    l += winLph_[i];
    if (std::isnan(winSpeed_[i])) speedOk = false; else v += winSpeed_[i];
  }
  if (t > 0) {
    out_.instLph = out_.fuelCut ? 0.0f : l / t;
    out_.instL100 = speedOk ? fuel::litersPer100(out_.instLph, v / t) : NAN;
  } else {
    out_.instLph = out_.instL100 = NAN;
  }

  out_.avg1 = st_.ring1.l100();
  out_.avg10 = st_.ring10.l100();
  out_.avg100 = st_.ring100.l100();
  out_.avgTank = avg::tankL100(static_cast<float>(st_.fillKm), static_cast<float>(st_.fillL), st_.prevFillL100);
  out_.avgTrip = st_.trip.active ? avg::simpleL100(static_cast<float>(st_.trip.km), static_cast<float>(st_.trip.liters)) : NAN;
  out_.avgProfile = avg::simpleL100(static_cast<float>(st_.totalKm), static_cast<float>(st_.totalL));
  out_.avgFills = st_.fills.l100();
  if (out_.source == fuel::Source::None) {
    // ohne Verbrauchsquelle (z. B. Diesel ohne 0x5E) gibt es keine Schnitte
    out_.avg1 = out_.avg10 = out_.avg100 = out_.avgTank = out_.avgTrip = out_.avgProfile = out_.avgFills = NAN;
  }

  // Tankinhalt: mit 0x2F aus dem geglätteten Füllstand, sonst Tankmodell (A7)
  const float level = s.fuelLevel.get(nowMs);
  if (s.link.pidSupported(0x2F) && !std::isnan(level)) {
    const float a = dtS / cfg::TANK_LEVEL_TAU_S;
    levelSmooth_ = std::isnan(levelSmooth_) ? level : levelSmooth_ + (a > 1 ? 1 : a) * (level - levelSmooth_);
    out_.tankL = levelSmooth_ / 100.0f * profile_.tankL;
  } else if (st_.tankModelValid) {
    out_.tankL = static_cast<float>(st_.tankModelL);
  } else {
    out_.tankL = NAN;
  }

  // Reichweite: nur die Prognose wird geglättet; unter 80 km vorsichtig mit dem höchsten Schnitt (A7)
  const float raw = fuel::prognosis(out_.avg100, out_.avg10, out_.avgFills);
  if (std::isnan(raw)) {
    out_.prognosis = NAN;
  } else if (std::isnan(out_.prognosis)) {
    out_.prognosis = raw;
  } else {
    const float a = dtS / cfg::RANGE_TAU_S;
    out_.prognosis += (a > 1 ? 1 : a) * (raw - out_.prognosis);
  }
  out_.rangeKm = fuel::rangeKm(out_.tankL, out_.prognosis);
  if (!std::isnan(out_.rangeKm) && out_.rangeKm < cfg::RANGE_CAUTIOUS_KM) {
    float worst = NAN;
    for (float x : {out_.avg100, out_.avg10, out_.avgFills})
      if (!std::isnan(x) && (std::isnan(worst) || x > worst)) worst = x;
    if (!std::isnan(worst)) out_.rangeKm = fuel::rangeKm(out_.tankL, worst);
  }

  const trip::TripState& tr = st_.trip;
  out_.tripKm = tr.active ? static_cast<float>(tr.km) : NAN;
  out_.tripL = tr.active ? static_cast<float>(tr.liters) : NAN;
  out_.tripCost = (tr.active && tr.costKnown) ? static_cast<float>(tr.cost) : NAN;
  out_.tripDurationS = tr.active ? static_cast<float>(tr.durationS) : NAN;
  out_.mixPrice = st_.mixPrice;
  out_.pumpPrice = st_.pumpPrice;
  out_.fillKm = static_cast<float>(st_.fillKm);
  out_.fillL = static_cast<float>(st_.fillL);
  out_.tripIdleS = tr.active ? static_cast<float>(tr.idleS) : NAN;
  out_.sinceFullL = static_cast<float>(st_.hadFullFill ? st_.calComputedL : st_.fillL);
  // Spar-Ziel (A9): aus, auto (beim Tanken gesetzt; bis zur ersten Füllung aus dem Gesamtschnitt) oder fest
  out_.goalBase = NAN;
  if (goalMode_ == 1) {
    const float fills = st_.fills.l100();
    out_.goalBase = std::isnan(fills) ? out_.avgProfile : fills;
    out_.goalL100 = std::isnan(st_.autoGoal) ? autoGoalFrom(out_.goalBase) : st_.autoGoal;
  } else if (goalMode_ == 2) {
    out_.goalL100 = goalFix_;
  } else {
    out_.goalL100 = NAN;
  }
  out_.thermoActive = st_.thermoActive != 0;
  // Wartung (Z 9)
  if (std::isnan(st_.odoOffsetKm)) {
    out_.odoKm = out_.oilLeftKm = out_.inspLeftKm = NAN;
  } else {
    out_.odoKm = static_cast<float>(st_.totalKm + st_.odoOffsetKm);
    out_.oilLeftKm = st_.oilDueKm - out_.odoKm;
    out_.inspLeftKm = st_.inspDueKm - out_.odoKm;
  }
}

bool VehicleCalc::takeTripRecord(trip::TripRecord& r) {
  if (!hasTripRecord_) return false;
  r = tripRecord_;
  hasTripRecord_ = false;
  return true;
}

bool VehicleCalc::takeProfileChanged() {
  const bool c = profileChanged_;
  profileChanged_ = false;
  return c;
}

bool VehicleCalc::saveDue(uint32_t nowMs) const {
  if (!active_) return false;
  if (saveNow_) return true;
  // regelmäßig nur, wenn sich etwas getan hat (Flash schonen, wenn das Display dauerhaft Strom hat)
  if (nowMs - lastSaveMs_ >= cfg::SAVE_PERIOD_MS && (st_.totalKm != kmAtSave_ || st_.totalL != litersAtSave_))
    return true;
  return standstillSinceMs_ != 0 && !standstillSaved_ && nowMs - standstillSinceMs_ >= cfg::SAVE_STANDSTILL_MS;
}

void VehicleCalc::markSaved(uint32_t nowMs) {
  lastSaveMs_ = nowMs;
  kmAtSave_ = st_.totalKm;
  litersAtSave_ = st_.totalL;
  saveNow_ = false;
  // zählt nur als Stillstands-Speicherung, wenn der Halt schon 10 s dauert
  if (standstillSinceMs_ != 0 && nowMs - standstillSinceMs_ >= cfg::SAVE_STANDSTILL_MS) standstillSaved_ = true;
  st_.seq++;
}

trip::FillRecord VehicleCalc::refuel(float liters, float price, bool full, trip::FillSource src, bool& calApplied) {
  calApplied = false;
  trip::FillRecord r;
  memset(&r, 0, sizeof(r));
  r.number = ++st_.lastFillNumber;
  r.profileId = profile_.id;
  r.full = full ? 1 : 0;
  r.date = gpsDate_;
  r.kmSinceLast = static_cast<float>(st_.fillKm);
  r.liters = liters;
  r.price = price;
  r.l100 = st_.fillKm > 0 ? static_cast<float>(st_.fillL / st_.fillKm * 100.0) : NAN;
  // ANNAHME: Kosten je 100 km der abgelaufenen Strecke mit dem Mischpreis, der dort galt
  r.costPer100 = std::isnan(r.l100) ? NAN : r.l100 * (std::isnan(st_.mixPrice) ? price : st_.mixPrice);
  r.source = src;

  const float rest = out_.tankL;
  if (!std::isnan(price)) {  // ohne Preis bleibt der bisherige Mischpreis
    st_.mixPrice = fuel::mixPrice(rest, st_.mixPrice, liters, price);
    st_.pumpPrice = price;
  }

  // ANNAHME: Ohne bekannten Rest ergibt erst eine Vollbetankung ein gültiges Tankmodell.
  const float model = fuel::tankAfterRefuel(rest, liters, full, profile_.tankL);
  if (!std::isnan(model)) {
    st_.tankModelL = model;
    st_.tankModelValid = 1;
  }
  levelSmooth_ = NAN;  // Füllstand nach dem Tanken neu einschwingen lassen
  st_.levelAtStopPct = NAN;  // nicht beim nächsten Start noch einmal als Tankvorgang erkennen

  // Kalibrierung nur zwischen Vollbetankungen (A7)
  st_.calFilledL += liters;
  if (full) {
    if (st_.hadFullFill) {
      const float cal = fuel::calibrate(profile_.fuelCal, static_cast<float>(st_.calFilledL),
                                        static_cast<float>(st_.calComputedL), static_cast<float>(st_.calKm), calApplied);
      if (calApplied && cal != profile_.fuelCal) {
        profile_.fuelCal = cal;
        profileChanged_ = true;
      }
    }
    st_.hadFullFill = 1;
    st_.calFilledL = st_.calComputedL = st_.calKm = 0;
  }

  if (st_.fillKm >= cfg::AVG_SIMPLE_MIN_KM) st_.prevFillL100 = r.l100;
  if (st_.fillKm > 0) st_.fills.push(static_cast<float>(st_.fillKm), static_cast<float>(st_.fillL));
  // Spar-Ziel auto wird bei jedem Tanken neu gesetzt (A9): Ø der letzten 5 Füllungen, ersatzweise Gesamt
  {
    const float fills = st_.fills.l100();
    st_.autoGoal = autoGoalFrom(std::isnan(fills) ? avg::simpleL100(static_cast<float>(st_.totalKm), static_cast<float>(st_.totalL)) : fills);
  }
  st_.fillKm = st_.fillL = 0;
  saveNow_ = true;
  return r;
}
