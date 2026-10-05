#include "live.h"

#include <esp_heap_caps.h>

#include <cmath>

#include "config.h"
#include "ui/values.h"

namespace live {

namespace {
constexpr int N = cfg::LIVE_ENTRIES;
Entry* buf = nullptr;
int head = -1;
int filled = 0;
uint32_t lastSpeedT = 0;
uint32_t counter = 0;
Entry empty = {};

struct Axis {
  float lo, hi;
  const char* label;
};
const Axis AXES[static_cast<int>(Ser::COUNT)] = {
    {0, 140, "Tempo"}, {0, 60, "Leistung"}, {0, 6000, "Drehzahl"}, {0, 100, "Gas"}, {-4, 4, "Beschl."},
};
}  // namespace

void init() {
  if (buf) return;
  buf = static_cast<Entry*>(heap_caps_malloc(sizeof(Entry) * N, MALLOC_CAP_SPIRAM));
}

void tick(const CarSnapshot& s) {
  if (!buf || s.speed.t == lastSpeedT || !s.speed.fresh(s.now)) return;
  lastSpeedT = s.speed.t;
  head = (head + 1) % N;
  if (filled < N) filled++;
  Entry& e = buf[head];
  e.t = s.speed.t;
  e.v[static_cast<int>(Ser::Speed)] = s.speed.get(s.now);
  e.v[static_cast<int>(Ser::Kw)] = s.powerKw.get(s.now);
  e.v[static_cast<int>(Ser::Rpm)] = s.rpm.get(s.now);
  e.v[static_cast<int>(Ser::Pedal)] = values::value(values::Key::Pedal, s);
  const float a = s.accel.get(s.now);
  e.v[static_cast<int>(Ser::Acc)] = std::isnan(a) ? NAN : (a < -5 ? -5 : (a > 5 ? 5 : a));
  counter++;
}

int count() { return filled; }

const Entry& at(int i) {
  if (!buf || i < 0 || i >= filled) return empty;
  return buf[(head - (filled - 1 - i) + N) % N];
}

uint32_t seq() { return counter; }

float lo(Ser r) { return AXES[static_cast<int>(r)].lo; }
float hi(Ser r) { return AXES[static_cast<int>(r)].hi; }
const char* label(Ser r) { return AXES[static_cast<int>(r)].label; }

}  // namespace live
