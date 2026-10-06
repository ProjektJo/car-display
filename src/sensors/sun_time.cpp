#include "sun_time.h"

#include <cmath>

#include "config.h"

namespace suntime {

namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double DEG = PI / 180.0;
}  // namespace

int64_t daysFromCivil(int y, int m, int d) {
  // Howard Hinnant, "chrono-Compatible Low-Level Date Algorithms"
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

int weekday(int y, int m, int d) {
  const int64_t z = daysFromCivil(y, m, d);
  return static_cast<int>(z >= -4 ? (z + 4) % 7 : (z + 5) % 7 + 6);
}

int64_t toEpoch(const DateTime& t) {
  return daysFromCivil(t.year, t.month, t.day) * 86400 + t.hour * 3600 + t.minute * 60 + t.second;
}

DateTime fromEpoch(int64_t s) {
  int64_t z = s >= 0 ? s / 86400 : (s - 86399) / 86400;
  int64_t rem = s - z * 86400;
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  DateTime t;
  t.day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  t.month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  t.year = static_cast<int>(yoe + era * 400 + (t.month <= 2));
  t.hour = static_cast<int>(rem / 3600);
  t.minute = static_cast<int>(rem / 60 % 60);
  t.second = static_cast<int>(rem % 60);
  return t;
}

// letzter Sonntag eines Monats (Tag)
static int lastSunday(int y, int m) {
  const int days = m == 3 ? 31 : 31;  // März und Oktober haben 31 Tage
  const int wd = weekday(y, m, days);
  return days - wd;
}

DateTime toLocal(const DateTime& utc) {
  const int64_t t = toEpoch(utc);
  const int64_t dstStart = toEpoch(DateTime{utc.year, 3, lastSunday(utc.year, 3), 1, 0, 0});
  const int64_t dstEnd = toEpoch(DateTime{utc.year, 10, lastSunday(utc.year, 10), 1, 0, 0});
  const int offsetH = (t >= dstStart && t < dstEnd) ? 2 : 1;
  return fromEpoch(t + offsetH * 3600);
}

void sunTimes(int year, int month, int day, float latDeg, float lonDeg, float& riseMin, float& setMin, int& polar) {
  // NOAA "General Solar Position Calculations", Mittag als Bezug
  const int n = static_cast<int>(daysFromCivil(year, month, day) - daysFromCivil(year, 1, 1)) + 1;
  const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  const double gamma = 2 * PI / (leap ? 366 : 365) * (n - 1);
  const double eqtime = 229.18 * (0.000075 + 0.001868 * std::cos(gamma) - 0.032077 * std::sin(gamma) -
                                  0.014615 * std::cos(2 * gamma) - 0.040849 * std::sin(2 * gamma));
  const double decl = 0.006918 - 0.399912 * std::cos(gamma) + 0.070257 * std::sin(gamma) - 0.006758 * std::cos(2 * gamma) +
                      0.000907 * std::sin(2 * gamma) - 0.002697 * std::cos(3 * gamma) + 0.00148 * std::sin(3 * gamma);
  const double lat = latDeg * DEG;
  const double c = std::cos(90.833 * DEG) / (std::cos(lat) * std::cos(decl)) - std::tan(lat) * std::tan(decl);
  polar = 0;
  if (c > 1) {
    polar = -1;  // Sonne geht nicht auf
    riseMin = setMin = NAN;
    return;
  }
  if (c < -1) {
    polar = 1;  // Sonne geht nicht unter
    riseMin = setMin = NAN;
    return;
  }
  const double ha = std::acos(c) / DEG;
  riseMin = static_cast<float>(720 - 4 * (lonDeg + ha) - eqtime);
  setMin = static_cast<float>(720 - 4 * (lonDeg - ha) - eqtime);
}

float nightFactor(const DateTime& utc, float latDeg, float lonDeg) {
  float rise = 0, set = 0;
  int polar = 0;
  sunTimes(utc.year, utc.month, utc.day, latDeg, lonDeg, rise, set, polar);
  if (polar > 0) return 0.0f;
  if (polar < 0) return 1.0f;
  float m = utc.hour * 60 + utc.minute + utc.second / 60.0f;
  // Bezug auf den Sonnentag: Zeiten können vor 0:00 bzw. nach 24:00 UTC liegen
  if (m < rise - 720) m += 1440;
  if (m > set + 720) m -= 1440;
  const float half = cfg::SUN_TRANSITION_MIN / 2.0f;
  auto ramp = [&](float x) { return x < -half ? 0.0f : (x > half ? 1.0f : (x + half) / (2 * half)); };
  // vor Sonnenaufgang Nacht, danach Tag; nach Sonnenuntergang wieder Nacht
  const float day = ramp(m - rise) * (1.0f - ramp(m - set));
  return 1.0f - day;
}

}  // namespace suntime
