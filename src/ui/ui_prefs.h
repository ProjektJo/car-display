// Einstellungen der Oberfläche im RAM (Kacheln, Großanzeige, Diagramme). Lesen beim Start aus NVS,
// Schreiben über storageTask (nur storageTask schreibt in den Flash).
#pragma once
#include "core/ui_settings.h"

namespace uiprefs {

void load();          // einmal beim Start (uiTask)
UiSettings& get();
void save();          // nach jeder Änderung

}  // namespace uiprefs
