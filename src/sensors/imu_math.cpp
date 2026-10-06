#include "imu_math.h"

#include "config.h"

namespace imu {

void Learner::reset() { *this = Learner(); }

void Learner::add(Vec a, Vec g, float speedKmh, float obdAccel, float dtS) {
  if (axes_.valid) return;
  const bool still = !std::isnan(speedKmh) && speedKmh < 0.5f && norm(g) < cfg::IMU_STILL_GYRO_RAD_S;
  // 1. "oben": 3 s Stillstand, Schwerkraft mitteln
  if (!upDone_) {
    if (still) {
      upSum_ = upSum_ + a * dtS;
      upT_ += dtS;
      if (upT_ >= cfg::IMU_LEARN_UP_S) {
        axes_.up = unit(upSum_);
        upDone_ = true;
      }
    } else {
      upSum_ = Vec{};
      upT_ = 0;
    }
    return;
  }
  // 2. "vorne": geradeaus beschleunigen (OBD-Tempo steigt deutlich, Gyro dreht kaum)
  const bool straight = norm(g) < cfg::IMU_STRAIGHT_GYRO_RAD_S;
  if (!std::isnan(obdAccel) && obdAccel > cfg::IMU_LEARN_FWD_MIN_MS2 && straight && !std::isnan(speedKmh) && speedKmh < 50) {
    // Anteil quer zur Schwerkraft = Beschleunigung nach vorne
    const Vec r = a - axes_.up * dot(a, axes_.up);
    fwdSum_ = fwdSum_ + r * dtS;
    fwdT_ += dtS;
    if (fwdT_ >= cfg::IMU_LEARN_FWD_S) {
      axes_.fwd = unit(fwdSum_);
      axes_.valid = norm(axes_.fwd) > 0.5f;
    }
  }
}

void Fusion::add(Vec a, Vec g, float speedKmh, float obdAccel, float dtS) {
  if (!axes_.valid || !(dtS > 0)) return;
  // Gyro-Nullpunkt bei jedem Stillstand über 3 s neu mitteln (Drift)
  const bool still = !std::isnan(speedKmh) && speedKmh < 0.5f;
  if (still) {
    standSum_ = standSum_ + g * dtS;
    standT_ += dtS;
    if (standT_ >= cfg::IMU_BIAS_STAND_S) {
      bias_ = standSum_ * (1.0f / standT_);
      standSum_ = Vec{};
      standT_ = 0;
    }
  } else {
    standSum_ = Vec{};
    standT_ = 0;
  }
  const Vec gc = g - bias_;
  const Vec left = axes_.left();
  const float sensorLong = dot(a, axes_.fwd);  // = a_echt + g · sin(Steigung)
  // Fahrzeugvibration glätten (τ wie die OBD-Beschleunigung)
  const float kf = dtS / (cfg::ACCEL_SMOOTH_TAU_S + dtS);
  lat_ += kf * (dot(a, left) - lat_);
  // Steigung: schnell über die Nick-Drehrate (Drehung um die Querachse, Nase hoch = bergauf),
  // langsam über den Vergleich mit der OBD-Beschleunigung (frei von Steigung)
  slope_ -= dot(gc, left) * dtS;
  if (!std::isnan(obdAccel)) {
    float s = (sensorLong - obdAccel) / cfg::GRAVITY;
    s = s < -1 ? -1 : (s > 1 ? 1 : s);
    const float meas = std::asin(s);
    const float k = dtS / (cfg::IMU_SLOPE_TAU_S + dtS);
    slope_ += k * (meas - slope_);
  } else if (still) {
    // im Stand: Abweichung der Schwerkraft von der gelernten Lage = Steigung des Standplatzes
    float s = sensorLong / cfg::GRAVITY;
    s = s < -1 ? -1 : (s > 1 ? 1 : s);
    const float k = dtS / (cfg::IMU_SLOPE_TAU_S + dtS);
    slope_ += k * (std::asin(s) - slope_);
  }
  long_ += kf * (sensorLong - cfg::GRAVITY * std::sin(slope_) - long_);
}

}  // namespace imu
