// Fahrzeugprofile in der Oberfläche (A6, U Menü "Diagnose"):
// - "Welches Fahrzeug?": Liste der Profile und "Neues Fahrzeug"; öffnet sich von selbst, wenn
//   kein Profil eindeutig zum Auto passt, und über Menü → Diagnose → Fahrzeug
// - Assistent "Neues Fahrzeug": Name, Kraftstoff, Hubraum, Tankgröße
// Die Vorschau hat dafür kein Layout; die Fenster folgen dem Stil von Menü und Diagnose.
#pragma once
#include "core/car_state.h"

namespace vehicledlg {

void openChooser();
void openWizard();

// Je Snapshot: öffnet die Auswahl einmal je neuer Frage
void update(const CarSnapshot& s);

}  // namespace vehicledlg
