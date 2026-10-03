#pragma once
#include <Arduino.h>
#include "config.h"

// Ringpuffer für Diagrammverläufe. NAN = kein Wert zu diesem Zeitpunkt.
class History {
 public:
  void push(float v) {
    buf_[head_] = v;
    head_ = (head_ + 1) % HISTORY_LEN;
    if (count_ < HISTORY_LEN) count_++;
  }
  uint16_t size() const { return count_; }
  // i = 0 ist der älteste Wert
  float at(uint16_t i) const {
    return buf_[(head_ + HISTORY_LEN - count_ + i) % HISTORY_LEN];
  }
  // Min/Max der gültigen Werte; false, wenn es keine gibt
  bool range(float& lo, float& hi) const {
    bool any = false;
    for (uint16_t i = 0; i < count_; i++) {
      float v = at(i);
      if (isnan(v)) continue;
      if (!any) { lo = hi = v; any = true; }
      lo = min(lo, v);
      hi = max(hi, v);
    }
    return any;
  }

 private:
  float buf_[HISTORY_LEN];
  uint16_t head_ = 0, count_ = 0;
};
