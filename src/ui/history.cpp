#include "history.h"

#include <esp_heap_caps.h>

#include <cmath>

namespace history {

namespace {
float* data = nullptr;   // [Reihe][CAPACITY], Ring
int head = -1;           // jüngster Eintrag
int filled = 0;
uint32_t lastSecond = 0;
uint32_t counter = 0;
}  // namespace

void init() {
  if (data) return;
  data = static_cast<float*>(heap_caps_malloc(sizeof(float) * values::SERIES_COUNT * CAPACITY, MALLOC_CAP_SPIRAM));
  if (!data) return;
  for (int i = 0; i < values::SERIES_COUNT * CAPACITY; i++) data[i] = NAN;
}

void tick(const CarSnapshot& s) {
  if (!data) return;
  const uint32_t sec = s.now / 1000;
  if (sec == lastSecond) return;
  // Bei einer Lücke (UI hing) die fehlenden Sekunden als "kein Wert" eintragen
  uint32_t steps = lastSecond ? sec - lastSecond : 1;
  if (steps > CAPACITY) steps = CAPACITY;
  lastSecond = sec;
  for (uint32_t n = 0; n < steps; n++) {
    head = (head + 1) % CAPACITY;
    if (filled < CAPACITY) filled++;
    for (int r = 0; r < values::SERIES_COUNT; r++)
      data[r * CAPACITY + head] = n + 1 == steps ? values::seriesValue(static_cast<values::Series>(r), s) : NAN;
  }
  counter++;
}

int count() { return filled; }

float at(values::Series r, int ageS) {
  if (!data || r < values::Series::Inst || r >= values::Series::COUNT || ageS < 0 || ageS >= filled) return NAN;
  const int i = (head - ageS + CAPACITY) % CAPACITY;
  return data[static_cast<int>(r) * CAPACITY + i];
}

uint32_t seq() { return counter; }

}  // namespace history
