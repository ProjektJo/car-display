// calcTask (Core 0): Verbrauch, Mittelwerte, Tank, Eco-Logik (A5). Die Rechnungen selbst
// liegen als reines C++ in calc/*, diese Task verbindet sie mit dem CarState und dem Speicher.
#pragma once

#include "calc/persist.h"
#include "core/profile.h"

namespace calc {

void init();  // vor dem Start der Tasks
void task(void* arg);
void step();  // ein Rechenschritt; die Task ruft ihn alle CALC_PERIOD_MS auf (der PC-Prüfstand direkt)

// Von storageTask: Profil laden (saved = gespeicherte Summen oder nullptr für ein neues Profil).
// p = nullptr: nichts mehr rechnen, bis ein Profil feststeht ("Welches Fahrzeug?").
// Die Daten werden kopiert; calcTask übernimmt sie im nächsten Schritt.
void requestLoad(const Profile* p, const PersistState* saved);

}  // namespace calc
