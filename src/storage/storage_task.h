// storageTask (Core 1, niedrige Priorität, A5): schreibt in den Flash und verwaltet die Profile.
// - erkennt das Profil zum verbundenen Auto (A6) und lädt es in die Rechnung (calcTask)
// - fragt "Welches Fahrzeug?", wenn kein Profil eindeutig passt
// - speichert laufende Summen (A/B), Fahrten, Tankfüllungen und geänderte Profile
// Andere Tasks rufen nur die Funktionen hier auf; der Flash wird ausschließlich von storageTask beschrieben.
#pragma once
#include <cstdint>

#include "calc/persist.h"
#include "calc/trip.h"
#include "core/profile.h"
#include "core/ui_settings.h"

namespace storage {

void init();  // vor dem Start der Tasks
void task(void*);
// Teile der Task: begin() einmal, poll() alle 50 ms (der PC-Prüfstand ruft sie direkt auf)
void begin();
void poll();

// Von calcTask
void requestSave(const PersistState& st);
void saveProfile(const Profile& p);
void appendTrip(uint8_t profileId, const trip::TripRecord& r);
void appendFill(uint8_t profileId, const trip::FillRecord& r);

// Von der UI ("Welches Fahrzeug?", Assistent "Neues Fahrzeug")
void chooseProfile(uint8_t id);       // Profil gewählt: es merkt sich VIN, PID-Liste und Protokoll dieses Autos
void dismissChoice();                 // Auswahl ohne Wahl geschlossen
void createProfile(const Profile& p); // neues Profil aus dem Assistenten
int summaries(ProfileSummary* out, int max);  // Liste der Profile (Kopie)
void saveUi(const UiSettings& ui);    // Kacheln, Großanzeige, Diagramme (NVS)

// Historie (uiTask): Fahrtenbuch und Tankfüllungen des geladenen Profils lesen lassen. Das Ergebnis steht
// danach in history(); historySeq() zählt jedes fertige Lesen hoch.
struct History {
  int nTrips = 0;
  int nFills = 0;
  trip::TripRecord* trips = nullptr;  // PSRAM, cfg::TRIP_LOG_SIZE Einträge, ältester zuerst
  trip::FillRecord* fills = nullptr;  // PSRAM, cfg::FILL_LOG_SIZE Einträge
};
void requestHistory();
uint16_t historySeq();
// Nur lesen, solange lockHistory() gehalten wird
const History& lockHistory();
void unlockHistory();

}  // namespace storage
