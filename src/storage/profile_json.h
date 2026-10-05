// Fahrzeugprofil als JSON-Datei (A6), mit ArduinoJson 7.
// Felder wie im Beispiel der Architektur; dazu "protocol_id" (gefundenes Protokoll) für die Erkennung.
// "supported_pids" steht wie in den Antworten 0100, 0120 … als 8 × 8 Hex-Zeichen.
#pragma once
#include <cstddef>

#include "core/profile.h"

// Schreibt das Profil nach out; liefert die Länge (0 = zu kleiner Puffer)
size_t profileToJson(const Profile& p, char* out, size_t size);

// Liest ein Profil; fehlende Felder bekommen Standardwerte. p.id bleibt, wie es war.
bool profileFromJson(const char* json, Profile& p);
