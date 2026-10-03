// Zahlenformat für die Anzeige: Dezimalkomma, Tausenderpunkt ab 1000, "–" ohne Wert (M, Gestaltung).
// Reines C++, nativ testbar.
#pragma once
#include <cstddef>

namespace fmt {

// Text für "kein Wert" (Gedankenstrich, UTF-8)
constexpr const char* NO_VALUE = "\xE2\x80\x93";

// Schreibt v mit 'decimals' Nachkommastellen (0–3) nach out.
// Beispiele: 1234.5 -> "1.234,5", 6.34 -> "6,3", NAN -> "–", -0.04 -> "0,0".
// Gibt die Länge zurück (ohne Nullbyte). Zu kleiner Puffer wird sicher abgeschnitten.
size_t number(char* out, size_t size, float v, int decimals);

}  // namespace fmt
