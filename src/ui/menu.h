// Menü und Unterdialoge (U Menü): Helligkeit, Spar-Ziel, Fahrzeugart, Wartung, Diagnose, Getankt,
// Kacheln zurücksetzen, Spartipps, Kalt-Grenze, Auto-Sprint, Fahrt beenden, Info.
#pragma once
#include "core/car_state.h"

namespace menu {

void open();
void openDiagnose();

// Einmal je Snapshot: aktualisiert die Werte im offenen Menü bzw. Dialog
void update(const CarSnapshot& s);

}  // namespace menu
