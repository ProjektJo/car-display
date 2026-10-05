// Menü und Unterdialoge (U Menü). Etappe 2: nur die Zeile Diagnose mit dem Diagnose-Dialog;
// die übrigen Zeilen und Dialoge folgen in Etappe 7.
#pragma once
#include "core/car_state.h"

namespace menu {

void open();
void openDiagnose();

// Einmal je Snapshot: aktualisiert die Werte im offenen Menü bzw. Dialog
void update(const CarSnapshot& s);

}  // namespace menu
