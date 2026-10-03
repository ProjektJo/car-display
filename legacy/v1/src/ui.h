#pragma once
#include "obd.h"

namespace ui {
void begin();
void nextPage();
// Zeichnet die aktuelle Seite neu, soweit nötig. Nicht blockierend.
void update(const CarData& car);
}  // namespace ui
