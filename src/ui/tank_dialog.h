// Tank-Fenster (U Tank-Fenster, A7 Tankfüllung): getankte Liter und Preis pro Liter mit ▲/▼ je Stelle,
// Ziffernfeld beim Tippen auf die Ziffern, Schalter "vollgetankt", Kosten live. Liegt über jeder Seite und
// schließt nicht von selbst. Öffnet sich automatisch, wenn calcTask einen Tankvorgang erkennt (nur mit 0x2F).
#pragma once
#include "core/car_state.h"

namespace tankdlg {

// Von Hand ("Getankt" auf Fahrt & Tank bzw. im Menü): Liter aus dem berechneten Verbrauch
void openManual(const CarSnapshot& s);

// Je Snapshot: öffnet das Fenster, wenn ein Tankvorgang erkannt wurde
void update(const CarSnapshot& s);

}  // namespace tankdlg
