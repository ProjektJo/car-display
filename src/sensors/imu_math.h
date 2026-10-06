// Beschleunigungssensor MPU6050 (A10): Einbaulage lernen, Längs-/Quer-G, Steigung, Gyro-Nullpunkt.
// Reines C++ ohne Arduino, nativ testbar. Messwerte in m/s² bzw. rad/s im Sensor-Koordinatensystem.
#pragma once
#include <cmath>
#include <cstdint>

namespace imu {

struct Vec {
  float x = 0, y = 0, z = 0;
};
inline Vec operator+(Vec a, Vec b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec operator-(Vec a, Vec b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec operator*(Vec a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec cross(Vec a, Vec b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float norm(Vec a) { return std::sqrt(dot(a, a)); }
inline Vec unit(Vec a) {
  const float n = norm(a);
  return n > 1e-6f ? a * (1.0f / n) : Vec{};
}

// Gelernte Einbaulage: "oben" (Gegenrichtung der Schwerkraft) und "vorne", beide Einheitsvektoren
struct Axes {
  Vec up;
  Vec fwd;
  bool valid = false;
  Vec left() const { return cross(up, fwd); }
};

// Lernt die Einbaulage (A10): 3 s Stillstand ergibt "oben", die erste geradeaus Beschleunigung aus dem
// Stand ergibt "vorne". Danach valid.
class Learner {
 public:
  void reset();
  // Je Probe (100 Hz): Beschleunigung m/s², Drehrate rad/s, OBD-Tempo und dessen Beschleunigung
  void add(Vec accel, Vec gyro, float speedKmh, float obdAccelMs2, float dtS);
  bool done() const { return axes_.valid; }
  const Axes& axes() const { return axes_; }
  bool hasUp() const { return upDone_; }

 private:
  bool upDone_ = false;
  Vec upSum_;
  float upT_ = 0;
  Vec fwdSum_;
  float fwdT_ = 0;
  Axes axes_;
};

// Rechnet mit gelernter Lage: Längs- und Quer-Beschleunigung, Steigung (Komplementärfilter aus
// Sensor-Längsbeschleunigung minus OBD-Beschleunigung und der Nick-Drehrate), Gyro-Nullpunkt im Stand.
class Fusion {
 public:
  void setAxes(const Axes& a) { axes_ = a; }
  // Je Probe: Rohwerte, OBD-Tempo und -Beschleunigung (NAN ohne), dt in s
  void add(Vec accel, Vec gyro, float speedKmh, float obdAccelMs2, float dtS);
  float longMs2() const { return long_; }   // Längsbeschleunigung ohne Steigungsanteil (+ = schneller)
  float latMs2() const { return lat_; }     // Querbeschleunigung (+ = nach links)
  float slopePct() const { return std::tan(slope_) * 100.0f; }
  float slopeRad() const { return slope_; }
  Vec gyroBias() const { return bias_; }

 private:
  Axes axes_;
  float slope_ = 0;       // rad, + = bergauf
  float long_ = 0, lat_ = 0;
  Vec bias_;
  Vec standSum_;
  float standT_ = 0;
};

}  // namespace imu
