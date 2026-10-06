#include "simulator.h"

#include <algorithm>
#include <cmath>

#include "config.h"

namespace {

using Mode = DriveSim::Mode;

// Fahrzyklus (ca. 4,5 min), wiederholt sich: Dauer s, Tempo am Ende km/h, Fahrweise
struct Segment {
  float durS;
  float vEnd;
  Mode mode;
};
constexpr Segment CYCLE[] = {
    {10, 0, Mode::Idle},          //  0 Stand an der Ampel
    {12, 50, Mode::Accel},        //  1 Stadt: anfahren
    {20, 50, Mode::Cruise},       //  2
    {6, 30, Mode::Overrun},       //  3 Schub
    {6, 50, Mode::Accel},         //  4
    {12, 50, Mode::Cruise},       //  5
    {10, 0, Mode::Overrun},       //  6 Schub bis zur Ampel, unter 15 km/h ausgekuppelt
    {20, 0, Mode::Idle},          //  7 Ampel-Leerlauf (hier wird auch getankt)
    {20, 100, Mode::Accel},       //  8 Landstraße
    {25, 100, Mode::Cruise},      //  9
    {10, 97, Mode::Sail},         // 10 Segeln: ausgekuppelt, Tempo hält sich fast
    {15, 90, Mode::Cruise},       // 11
    {12, 60, Mode::Overrun},      // 12 Schub
    {15, 60, Mode::Cruise},       // 13
    {12, 0, Mode::CoastBrake},    // 14 ausgekuppelt bremsen bis zum Stand
    {65, 0, Mode::Idle},          // 15 langer Stand (Hinweis "Stand · Motor aus?" nach 60 s)
};
constexpr int CYCLE_LEN = sizeof(CYCLE) / sizeof(CYCLE[0]);
constexpr int SEG_REFUEL = 7;       // Abschnitt, in dem getankt wird
constexpr int SEG_AFTER_SPRINT = 9; // nach dem Sprint weiter mit Konstantfahrt 100 km/h
constexpr int SEG_LONG_STAND = 15;  // langer Stand: hier startet kein Sprint (Hinweis "Stand" prüfbar)

// Getriebe: km/h je 1000 U/min (A6, Renault Modus grob)
constexpr float GEAR_K[] = {7.3f, 13.2f, 19.5f, 26.0f, 32.0f};
constexpr int GEARS = 5;

// Fahrer und Motor
constexpr float RPM_IDLE_WARM = 780;
constexpr float RPM_IDLE_COLD_PER_K = 7;     // kalter Motor dreht höher: +7 U/min je K unter 50 °C
constexpr float COLD_IDLE_BELOW_C = 50;
constexpr float RPM_UPSHIFT_ACCEL = 2900;    // Fahrer schaltet beim Beschleunigen hier hoch
constexpr float RPM_MIN_CRUISE = 1700;       // höchster Gang, der noch so viel Drehzahl hat (Stadt 50 im 4.)
constexpr float RPM_FLOOR_ACCEL = 1150;
constexpr float RPM_FLOOR_CRUISE = 1100;
constexpr float CLUTCH_BELOW_KMH = 15;       // darunter wird ausgekuppelt
constexpr float STAND_BELOW_KMH = 3;
constexpr float ACCEL_CURVE_EXP = 1.6f;      // Beschleunigung lässt mit dem Tempo nach (Vorschau)
constexpr float SPRINT_START_MAX_KMH = 0.5f; // Sprint beginnt nur aus dem Stand

// Fahrer: Gaspedal % (wie in der Vorschau)
constexpr float PEDAL_ACCEL = 46, PEDAL_ACCEL_VAR = 4;
constexpr float PEDAL_CRUISE_BASE = 12, PEDAL_CRUISE_PER_KMH = 0.12f, PEDAL_CRUISE_VAR = 1.5f;
constexpr float PEDAL_FULL = 98, PEDAL_FULL_VAR = 1, PEDAL_SHIFT = 4;
// Der Fuß zittert langsam, nicht bei jeder Abfrage neu: Rauschen mit τ = 1 s geglättet (Eco-Score "ruhiges Gas")
constexpr float PEDAL_WOBBLE_TAU_S = 1.0f;
constexpr float PEDAL_WOBBLE_GAIN = 2.5f;     // gleicht die kleinere Schwankung nach dem Glätten aus

// Verbrauchsmodell l/h (wie in der Vorschau): Grundwert + je km/h (+ je (km/h)² bei Konstantfahrt)
constexpr float LPH_IDLE = 0.7f;
constexpr float LPH_ACCEL_BASE = 2.5f, LPH_ACCEL_PER_KMH = 0.09f;
constexpr float LPH_CRUISE_BASE = 0.5f, LPH_CRUISE_PER_KMH = 0.035f, LPH_CRUISE_PER_KMH2 = 0.0003f;
constexpr float LPH_FULL_BASE = 6, LPH_FULL_PER_KMH = 0.16f, LPH_REV_STAND = 4, LPH_SHIFT = 1;
constexpr float LPH_NOISE = 0.04f;           // ±4 %
constexpr float LPH_COLD_IDLE_PER_K = 0.01f; // Leerlauf kalt: +0,01 l/h je K unter 60 °C
constexpr float COLD_BELOW_C = 60;
constexpr float COLD_EXTRA_MAX = 0.15f;      // bis +15 % Verbrauch bei kaltem Motor
constexpr float COLD_EXTRA_SPAN_K = 45;

// Sprint
constexpr float SPRINT_REV_S = 1.5f;         // so lange wird im Stand hochgedreht
constexpr float SPRINT_VMAX = 115;           // Ziel der Beschleunigungskurve km/h
constexpr float SPRINT_TAU_S = 6.5f;         // Zeitkonstante: 0–100 in ca. 13 s
constexpr float SPRINT_SHIFT_RPM = 5600;
constexpr float SPRINT_SHIFT_PAUSE_S = 0.4f; // Schaltpause, Gas kurz weg (bricht die Messung nicht ab)
constexpr float SPRINT_LAUNCH_RPM = 3800;    // Kupplung schleift beim Anfahren
constexpr float SPRINT_REV_RPM_PER_S = 2500; // Hochdrehen im Stand
constexpr float SPRINT_TARGET_KMH = 100;

// Ereignisse
constexpr float DTC_AT_S = 180;              // P0171 erscheint nach 3 min
constexpr uint16_t DTC_CODE = 0x0171;
constexpr float LTFT_LEAN_PCT = 12;          // Gemisch zu mager: Langzeitkorrektur läuft hoch
constexpr float LTFT_DRIFT_PER_S = 0.2f;
constexpr float REFUEL_AT_S = 450;           // erster Tankvorgang nach 7,5 min
constexpr float REFUEL_LOW_L = 8;            // später immer, wenn der Tank fast leer ist
constexpr float ENGINE_OFF_S = 30;           // so lange ist der Motor beim Tanken aus

// Umgebung
constexpr float AMBIENT_C = 12;
constexpr float COOLANT_THERMOSTAT_C = 90;
constexpr float COOLANT_WARMUP_TAU_S = 420;
constexpr float COOLANT_COOLDOWN_TAU_S = 1800;
constexpr float IAT_WARM_RISE_C = 10;
constexpr float IAT_RISE_TAU_S = 600;
constexpr float MAP_OVERRUN_KPA = 22;
constexpr float MAP_ENGINE_OFF_KPA = 99;
constexpr float MAP_MIN_KPA = 20, MAP_MAX_KPA = 99;
constexpr float R_AIR = 0.28705f;            // kJ/(kg·K) (A7)
constexpr float KELVIN = 273.15f;
constexpr float THROTTLE_CLOSED_PCT = 2.0f;  // Drosselklappe zu (Schub)
constexpr float THROTTLE_IDLE_PCT = 3.0f;
constexpr float THROTTLE_PER_PEDAL = 0.85f;
constexpr float LOAD_OVERRUN_PCT = 11;
constexpr float VOLT_RUNNING = 14.15f, VOLT_OFF = 12.4f;
constexpr float FUEL_LEVEL_STEP = 100.0f / 255.0f; // Auflösung von PID 0x2F
constexpr float FUEL_SLOSH_PCT = 1.5f;       // Tankanzeige schwappt während der Fahrt
constexpr float SPEED_NOISE_KMH = 0.4f, RPM_IDLE_NOISE = 15, IAT_NOISE = 0.2f, MAP_NOISE = 1;
constexpr float VOLT_NOISE_RUN = 0.06f, VOLT_NOISE_OFF = 0.03f;
constexpr uint8_t FUELSYS_OPEN_COLD = 1, FUELSYS_CLOSED = 2, FUELSYS_OVERRUN = 4;  // PID 0x03
constexpr float CLOSED_LOOP_FROM_C = 40;     // Lambdaregelung ab dieser Kühlmitteltemperatur
constexpr float STFT_VAR_PCT = 3;

float clampf(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }

}  // namespace

DriveSim::DriveSim(uint32_t seed) : rng_(seed ? seed : 1), nextAutoSprint_(cfg::SIM_AUTO_SPRINT_PERIOD_S) {
  startSegment(0);
  step(0);
}

float DriveSim::rnd(float amplitude) {
  // xorshift32, reproduzierbar
  rng_ ^= rng_ << 13;
  rng_ ^= rng_ >> 17;
  rng_ ^= rng_ << 5;
  const float u = static_cast<float>(rng_ & 0xFFFFFF) / static_cast<float>(0xFFFFFF);  // 0..1
  return (u * 2.0f - 1.0f) * amplitude;
}

void DriveSim::startSegment(int index) {
  seg_ = index % CYCLE_LEN;
  segT_ = 0;
  segV0_ = speed_;
  mode_ = CYCLE[seg_].mode;
  const bool wantRefuel = (t_ >= REFUEL_AT_S && !refuelled_) || tankL_ < REFUEL_LOW_L;
  if (seg_ == SEG_REFUEL && wantRefuel) {
    mode_ = Mode::EngineOff;
    offT_ = 0;
  }
}

float DriveSim::fuelForMap(float mapKpa, float rpm) const {
  // Speed-Density vorwärts (A7): Verbrauch, der zu diesem Saugrohrdruck gehört
  const float trim = out_.fuelSys == FUELSYS_CLOSED ? (out_.stftPct + out_.ltftPct) : 0.0f;
  const float airGs = mapKpa * DISPLACEMENT_L * rpm / 120.0f * VE / (R_AIR * (out_.iatC + KELVIN));
  return airGs / AFR * (1.0f + trim / 100.0f) * 3600.0f / DENSITY_G_PER_L;
}

float DriveSim::mapForFuel(float lph, float rpm) const {
  // Umkehrung der Speed-Density-Rechnung (A7): welcher Saugrohrdruck ergibt diesen Verbrauch?
  const float fuelGs = lph * DENSITY_G_PER_L / 3600.0f;
  const float trim = out_.fuelSys == FUELSYS_CLOSED ? (out_.stftPct + out_.ltftPct) : 0.0f;
  const float airGs = fuelGs * AFR / (1.0f + trim / 100.0f);
  const float iatK = out_.iatC + KELVIN;
  const float denom = DISPLACEMENT_L * rpm / 120.0f * VE;
  return denom > 0 ? clampf(airGs * R_AIR * iatK / denom, MAP_MIN_KPA, MAP_MAX_KPA) : MAP_ENGINE_OFF_KPA;
}

void DriveSim::step(float dtS) {
  t_ += dtS;

  // Ereignisse
  if (!dtcSet_ && !dtcCleared_ && t_ >= DTC_AT_S) {
    dtcSet_ = true;
    out_.mil = true;
    out_.dtcCount = 1;
    out_.dtc = DTC_CODE;
  }
  if (dtcSet_ && ltft_ < LTFT_LEAN_PCT) ltft_ = std::min(LTFT_LEAN_PCT, ltft_ + LTFT_DRIFT_PER_S * dtS);
  if (t_ >= nextAutoSprint_) {
    sprintRequested_ = true;
    nextAutoSprint_ += cfg::SIM_AUTO_SPRINT_PERIOD_S;
  }

  out_.iatC = AMBIENT_C + IAT_WARM_RISE_C * (1.0f - std::exp(-t_ / IAT_RISE_TAU_S)) + rnd(IAT_NOISE);

  if (mode_ == Mode::EngineOff)
    stepEngineOff(dtS);
  else
    stepDriving(dtS);

  // Tank
  tankL_ = std::max(0.0f, tankL_ - out_.trueLph * dtS / 3600.0f);
  float level = tankL_ / TANK_L * 100.0f + (speed_ > STAND_BELOW_KMH ? rnd(FUEL_SLOSH_PCT) : 0.0f);
  out_.fuelLevelPct = clampf(std::round(level / FUEL_LEVEL_STEP) * FUEL_LEVEL_STEP, 0, 100);
}

void DriveSim::stepEngineOff(float dtS) {
  offT_ += dtS;
  speed_ = 0;
  coolant_ += (AMBIENT_C - coolant_) * dtS / COOLANT_COOLDOWN_TAU_S;
  out_.engineOn = false;
  out_.speedKmh = 0;
  out_.rpm = 0;
  out_.gear = 0;
  out_.pedalPct = 0;
  out_.throttlePct = THROTTLE_CLOSED_PCT;
  out_.mapKpa = MAP_ENGINE_OFF_KPA;
  out_.loadPct = 0;
  out_.stftPct = 0;
  out_.fuelSys = 0;
  out_.trueLph = 0;
  out_.coolantC = coolant_;
  out_.voltage = VOLT_OFF + rnd(VOLT_NOISE_OFF);
  if (offT_ >= ENGINE_OFF_S) {
    tankL_ = TANK_L;  // vollgetankt
    refuelled_ = true;
    mode_ = CYCLE[SEG_REFUEL].mode;  // Motor an, Rest des Ampel-Abschnitts im Leerlauf
    segT_ = 0;
  }
}

void DriveSim::stepDriving(float dtS) {
  // Sprint starten, sobald das Auto steht
  if (sprintRequested_ && sprintT_ < 0 && speed_ < SPRINT_START_MAX_KMH && mode_ == Mode::Idle && seg_ != SEG_LONG_STAND) {
    sprintRequested_ = false;
    sprintT_ = 0;
    sprintGear_ = 1;
    shiftPauseUntil_ = -1;
    mode_ = Mode::Sprint;
  }

  const bool cold = coolant_ < COLD_BELOW_C;
  float pedal = 0, rpm = 0, lph = 0;
  pedalWobble_ += (rnd(1.0f) - pedalWobble_) * std::min(1.0f, dtS / PEDAL_WOBBLE_TAU_S);
  const float wobble = clampf(pedalWobble_ * PEDAL_WOBBLE_GAIN, -1.0f, 1.0f);
  uint8_t gear = 0;
  bool overrunCut = false;

  if (mode_ == Mode::Sprint) {
    sprintT_ += dtS;
    const bool pause = sprintT_ < shiftPauseUntil_;
    if (sprintT_ > SPRINT_REV_S) {
      const float accel = pause ? 0.0f : (SPRINT_VMAX - speed_) / SPRINT_TAU_S;  // km/h je s
      speed_ += accel * dtS;
    }
    const float gearRpm = speed_ / GEAR_K[sprintGear_ - 1] * 1000.0f;
    if (!pause && gearRpm > SPRINT_SHIFT_RPM && sprintGear_ < GEARS) {
      sprintGear_++;
      shiftPauseUntil_ = sprintT_ + SPRINT_SHIFT_PAUSE_S;
    }
    gear = sprintGear_;
    if (speed_ < STAND_BELOW_KMH)
      rpm = std::min(SPRINT_LAUNCH_RPM, RPM_IDLE_WARM + sprintT_ * SPRINT_REV_RPM_PER_S);
    else
      rpm = std::max(sprintGear_ == 1 ? SPRINT_LAUNCH_RPM : 0.0f, speed_ / GEAR_K[sprintGear_ - 1] * 1000.0f);
    pedal = pause ? PEDAL_SHIFT : PEDAL_FULL + rnd(PEDAL_FULL_VAR);
    lph = speed_ < STAND_BELOW_KMH ? LPH_REV_STAND : (pause ? LPH_SHIFT : LPH_FULL_BASE + LPH_FULL_PER_KMH * speed_);
    if (speed_ >= SPRINT_TARGET_KMH) {
      sprintT_ = -1;
      startSegment(SEG_AFTER_SPRINT);
    }
  } else {
    // Fahrzyklus
    const Segment& s = CYCLE[seg_];
    segT_ += dtS;
    float f = std::min(1.0f, segT_ / s.durS);
    if (s.mode == Mode::Accel) f = 1.0f - std::pow(1.0f - f, ACCEL_CURVE_EXP);
    speed_ = segV0_ + (s.vEnd - segV0_) * f;
    if (segT_ >= s.durS) startSegment(seg_ + 1);

    const bool declutched = speed_ < STAND_BELOW_KMH || mode_ == Mode::Idle || mode_ == Mode::Sail ||
                            mode_ == Mode::CoastBrake || (mode_ == Mode::Overrun && speed_ < CLUTCH_BELOW_KMH);
    const float idleRpm = RPM_IDLE_WARM + std::max(0.0f, COLD_IDLE_BELOW_C - coolant_) * RPM_IDLE_COLD_PER_K;
    if (declutched) {
      gear = 0;
      rpm = idleRpm + rnd(RPM_IDLE_NOISE);
      lph = LPH_IDLE + std::max(0.0f, COLD_BELOW_C - coolant_) * LPH_COLD_IDLE_PER_K;
    } else if (mode_ == Mode::Accel) {
      int g = 0;
      while (g < GEARS - 1 && speed_ / GEAR_K[g] * 1000.0f > RPM_UPSHIFT_ACCEL) g++;
      gear = static_cast<uint8_t>(g + 1);
      rpm = std::max(RPM_FLOOR_ACCEL, speed_ / GEAR_K[g] * 1000.0f);
      pedal = PEDAL_ACCEL + wobble * PEDAL_ACCEL_VAR;
      lph = LPH_ACCEL_BASE + LPH_ACCEL_PER_KMH * speed_;
    } else {  // Cruise, Overrun
      int g = GEARS - 1;
      while (g > 0 && speed_ / GEAR_K[g] * 1000.0f < RPM_MIN_CRUISE) g--;
      gear = static_cast<uint8_t>(g + 1);
      rpm = std::max(RPM_FLOOR_CRUISE, speed_ / GEAR_K[g] * 1000.0f);
      if (mode_ == Mode::Overrun) {
        overrunCut = true;
      } else {
        pedal = PEDAL_CRUISE_BASE + speed_ * PEDAL_CRUISE_PER_KMH + wobble * PEDAL_CRUISE_VAR;
        lph = LPH_CRUISE_BASE + LPH_CRUISE_PER_KMH * speed_ + LPH_CRUISE_PER_KMH2 * speed_ * speed_;
      }
    }
  }

  if (cold && lph > 0) lph *= 1.0f + COLD_EXTRA_MAX * std::min(1.0f, (COLD_BELOW_C - coolant_) / COLD_EXTRA_SPAN_K);
  if (lph > 0) lph *= 1.0f + rnd(LPH_NOISE);

  // Kühlmittel erwärmt sich bis zum Thermostat
  coolant_ += (COOLANT_THERMOSTAT_C - coolant_) * dtS / COOLANT_WARMUP_TAU_S;

  out_.engineOn = true;
  out_.gear = gear;
  out_.speedKmh = std::max(0.0f, speed_ + (speed_ > STAND_BELOW_KMH ? rnd(SPEED_NOISE_KMH) : 0.0f));
  out_.rpm = rpm;
  out_.pedalPct = clampf(pedal, 0, 100);
  out_.throttlePct = overrunCut ? THROTTLE_CLOSED_PCT : THROTTLE_IDLE_PCT + out_.pedalPct * THROTTLE_PER_PEDAL;
  out_.coolantC = coolant_;
  out_.voltage = VOLT_RUNNING + rnd(VOLT_NOISE_RUN);
  out_.ltftPct = ltft_;
  // Kraftstoffsystem: kalt offen (1), Schub (4), sonst geregelt (2)
  out_.fuelSys = overrunCut ? FUELSYS_OVERRUN : (coolant_ < CLOSED_LOOP_FROM_C ? FUELSYS_OPEN_COLD : FUELSYS_CLOSED);
  out_.stftPct = out_.fuelSys == FUELSYS_CLOSED ? rnd(STFT_VAR_PCT) : 0.0f;
  out_.trueLph = overrunCut ? 0.0f : lph;
  out_.mapKpa = overrunCut ? MAP_OVERRUN_KPA + rnd(MAP_NOISE) : mapForFuel(lph, rpm);
  // Mehr als Umgebungsdruck saugt der Motor nicht an: Ist der Druck gekappt, verbraucht er auch nur so viel
  if (!overrunCut && out_.mapKpa >= MAP_MAX_KPA) out_.trueLph = fuelForMap(out_.mapKpa, rpm);
  out_.loadPct = overrunCut ? LOAD_OVERRUN_PCT : clampf(out_.mapKpa / MAP_MAX_KPA * 100.0f, 0, 100);
}
