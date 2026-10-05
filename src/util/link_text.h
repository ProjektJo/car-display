// Texte zur Verbindung für Startbildschirm und Diagnose-Dialog (U Startbildschirm, U Menü Diagnose).
// Reines C++, nativ testbar.
#pragma once
#include <cstddef>

#include "core/car_state.h"

namespace linktext {

// Kurzer Satz zum Fehler, z. B. "Kein Adapter gefunden"; "" ohne Fehler
const char* error(LinkError e);

// Satz mit der Lösung, z. B. "Zündung an? Handy-App des Adapters schließen."
const char* hint(LinkError e);

// Unterstützte PIDs mit den fehlenden wichtigen, z. B. "13 (ohne MAF, 5E)"; "–" solange unbekannt
void supported(const LinkInfo& li, char* out, size_t size);

// Woraus der Verbrauch gerechnet wird (A7 Reihenfolge 0x5E, MAF, Saugrohrdruck)
const char* fuelSource(const LinkInfo& li);

// Schritte des Startbildschirms (U Startbildschirm): Adapter, Protokoll, Fahrzeug
enum class StepKind : uint8_t { Hidden, Current, Done, Failed };
struct Step {
  StepKind kind = StepKind::Hidden;
  char text[48] = "";
};
constexpr int STEP_COUNT = 3;
void startSteps(const LinkInfo& li, Step out[STEP_COUNT]);

}  // namespace linktext
