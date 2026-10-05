// Fenster, die sich nur im akuten Fall zeigen (U Start-Karte, U Fahrzeug-Prüfung):
// - Start-Karte "Letzte Fahrt" einmal nach dem Start (6 s, Tippen oder Losfahren schließt)
// - "Fährst du mit Fahrzeug …?", wenn die Gänge nicht zum Profil passen (A6), einmal je Einschalten
#pragma once
#include "core/car_state.h"

namespace ecopopups {

// Je Snapshot; ready = Startbildschirm ist weg
void update(const CarSnapshot& s, bool ready);

}  // namespace ecopopups
