// Einstellungen der Oberfläche, die in NVS gespeichert werden (U Seite 4, 5, 7):
// Belegung der sechs Kacheln, Wert der Großanzeige, Reihen und Zeitfenster der Diagramme.
// Werte als Schlüsselwörter (values::id), damit eine neue Firmware alte Einstellungen versteht.
#pragma once
#include <cstdint>

struct UiSettings {
  static constexpr int TILES = 6;
  char tiles[TILES][12] = {"speed", "rpm", "inst", "avg10", "pedal", "range"};  // Standard (U Seite 4)
  uint8_t big = 0;                      // Index in der Liste der Großanzeige
  char series[2][12] = {"inst", "speed"};  // Diagramme: zwei Reihen, zweite darf leer sein
  uint16_t windowS = 300;               // 1, 5 oder 30 min
  uint8_t sportPair = 0;                // Sport: Serienpaar im Live-Diagramm (A10)
  uint8_t reserved[3] = {};
};
