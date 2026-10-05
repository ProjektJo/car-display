// Werte für Kacheln, Großanzeige, Detail und Diagramme (U Seite 4, 5, 7; Vorschau "M").
// Jeder Wert hat Beschriftung, Einheit, Nachkommastellen, Farbe und ggf. eine Verlaufsreihe.
#pragma once
#include <cstddef>
#include <cstdint>

#include "core/car_state.h"

namespace values {

enum class Key : uint8_t {
  Speed, Rpm, Inst, Avg10, Avg100, AvgTank, AvgProfile, Pedal, Range, Coolant, Volt, Load, Map, Iat, Gear,
  COUNT
};
constexpr int COUNT = static_cast<int>(Key::COUNT);

// Verlaufsreihen (1 Wert je Sekunde, 30 min; Diagramme und Detail)
enum class Series : int8_t { None = -1, Inst, Speed, Rpm, Pedal, Coolant, Volt, COUNT };
constexpr int SERIES_COUNT = static_cast<int>(Series::COUNT);

const char* label(Key k);
const char* unit(Key k, const CarSnapshot& s);
int decimals(Key k);
Series series(Key k);
// Schlüssel für die Speicherung (NVS), stabil über Versionen
const char* id(Key k);
bool fromId(const char* id, Key& out);

// Rohwert (NAN ohne Wert)
float value(Key k, const CarSnapshot& s);
// Anzeige-Text (Dezimalkomma, Tausenderpunkt, "–")
void text(Key k, const CarSnapshot& s, char* out, size_t size);
// Farbe nach Bedeutung (theme::TEXT, GOOD, WARN)
uint32_t color(Key k, const CarSnapshot& s);

// Verlaufswert einer Reihe (für die Aufzeichnung)
float seriesValue(Series r, const CarSnapshot& s);
const char* seriesLabel(Series r);   // lang, z. B. "Tempo"
const char* seriesShort(Series r);   // Chip, z. B. "Drehz."
// Achse beginnt bei 0 (Tempo, Drehzahl, Gaspedal, Verbrauch)
bool seriesFromZero(Series r);
int seriesDecimals(Series r);

// Bezug für Verbrauchsfarben: Spar-Ziel, sonst Tank-Schnitt (M Farbschwellen)
float consumptionRef(const CarSnapshot& s);
uint32_t consumptionColor(float l100, const CarSnapshot& s);

}  // namespace values
