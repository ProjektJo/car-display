// Live-Puffer der Sport-Seite (A10): jede Abfrage der letzten 30 s (ca. 4–8 Hz, 240 Einträge) mit
// Tempo, Leistung, Drehzahl, Gaspedal und Beschleunigung. Liegt im PSRAM, nur uiTask schreibt und liest.
#pragma once
#include <cstdint>

#include "core/car_state.h"

namespace live {

enum class Ser : uint8_t { Speed, Kw, Rpm, Pedal, Acc, COUNT };

struct Entry {
  uint32_t t;   // ms
  float v[static_cast<int>(Ser::COUNT)];
};

void init();
// Je Snapshot: nimmt einen Eintrag auf, wenn eine neue Tempo-Messung da ist
void tick(const CarSnapshot& s);
int count();
// i = 0 ältester … count()-1 jüngster
const Entry& at(int i);
uint32_t seq();  // zählt jeden neuen Eintrag

// Feste Achsen je Reihe (A10): Tempo 0–140, kW 0–60, U/min 0–6000, Gas 0–100, Beschleunigung −4…+4
float lo(Ser r);
float hi(Ser r);
const char* label(Ser r);

}  // namespace live
