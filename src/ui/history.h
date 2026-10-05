// Verlauf der letzten 30 min, 1 Wert je Sekunde und Reihe (U Seite 7 Diagramme, U Bedienung Detail).
// Liegt im PSRAM. Nur uiTask schreibt und liest.
#pragma once
#include <cstdint>

#include "core/car_state.h"
#include "ui/values.h"

namespace history {

constexpr int CAPACITY = 1800;  // 30 min

void init();
// Einmal je Snapshot; nimmt jede volle Sekunde einen Wert je Reihe auf
void tick(const CarSnapshot& s);
// Anzahl gespeicherter Sekunden (höchstens CAPACITY)
int count();
// Wert vor ageS Sekunden (0 = jüngster), NAN ohne Wert
float at(values::Series r, int ageS);
// zählt jede neue Sekunde hoch (Diagramme zeichnen nur bei Änderung neu)
uint32_t seq();

}  // namespace history
