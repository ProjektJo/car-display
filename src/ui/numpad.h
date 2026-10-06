// Ziffernfeld als Fenster (z. B. Tachostand, Wartungsintervall): Titel, Eingabe, 3 × 4 Tasten,
// "Übernehmen" ruft done(Wert) auf. Ganze Zahlen; maxDigits begrenzt die Länge.
#pragma once
#include <cstdint>

namespace numpad {

using DoneFn = void (*)(float value);
// back = wird nach Übernehmen bzw. Abbrechen aufgerufen (z. B. zurück ins Menü), darf nullptr sein
void open(const char* title, const char* unit, float initial, int maxDigits, DoneFn done, void (*back)());

}  // namespace numpad
