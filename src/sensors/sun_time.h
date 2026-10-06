// Uhrzeit und Sonnenstand aus GPS (A9 Sonnenstand, Z 20). Reines C++, nativ testbar.
// - Ortszeit Deutschland (MEZ/MESZ, Sommerzeit nach EU-Regel) aus der GPS-Zeit (UTC)
// - Sonnenauf- und -untergang nach der NOAA-Näherung, 15 min Übergang Tag/Nacht
#pragma once
#include <cstdint>

namespace suntime {

struct DateTime {
  int year, month, day, hour, minute, second;
};

// Tage seit 1970-01-01 (proleptischer gregorianischer Kalender)
int64_t daysFromCivil(int y, int m, int d);
// 0 = Sonntag … 6 = Samstag
int weekday(int y, int m, int d);
// Sekunden seit 1970 aus Datum und Uhrzeit (UTC)
int64_t toEpoch(const DateTime& t);
DateTime fromEpoch(int64_t s);

// Ortszeit für Deutschland: UTC+1, im Sommer UTC+2 (letzter Sonntag im März 01:00 UTC bis
// letzter Sonntag im Oktober 01:00 UTC)
DateTime toLocal(const DateTime& utc);

// Sonnenaufgang und -untergang in Minuten nach Mitternacht UTC. polar: 1 = Polartag, -1 = Polarnacht, 0 sonst
void sunTimes(int year, int month, int day, float latDeg, float lonDeg, float& riseMin, float& setMin, int& polar);

// Nacht-Anteil 0 (Tag) … 1 (Nacht) mit 15 min Übergang um Sonnenauf- und -untergang
float nightFactor(const DateTime& utc, float latDeg, float lonDeg);

}  // namespace suntime
