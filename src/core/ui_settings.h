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
  // Menü (U Menü, Etappe 7)
  uint8_t tips = 1;                     // Spartipps an/aus
  uint8_t autoSprint = 1;               // Auto-Sprint an/aus
  uint8_t dayNight = 0;                 // Helligkeit: 0 Tag, 1 Nacht, 2 Auto (GPS)
  uint8_t brightDay = 80;               // 10–100 %, 10er Schritte (A2 Nr. 5)
  uint8_t brightNight = 25;             // 1–60 %, unter 5 % 1er, sonst 5er Schritte
  uint8_t goalMode = 0;                 // Spar-Ziel: 0 aus, 1 auto, 2 fest (Z 13)
  uint8_t flip180 = 0;                  // Bild und Touch um 180° gedreht (Einbaulage im Auto)
  float goalFix = 5.5f;                 // festes Ziel 3,0–7,0
};
