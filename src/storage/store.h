// Dateien im Flash (A8): Profile (JSON), laufende Summen (A/B mit Prüfsumme), Fahrtenbuch und
// Tankfüllungen auf LittleFS, Einstellungen in NVS. Nur storageTask ruft diese Funktionen auf.
//
// Ablage (im Simulator alles unter /sim, damit Testfahrten kein echtes Profil verfälschen):
//   /profiles/p<id>.json      Fahrzeugprofil
//   /data/p<id>_a.bin, _b.bin laufende Summen, abwechselnd geschrieben, die jüngste gültige gilt
//   /data/p<id>_trips.bin     letzte 50 Fahrten (Ringdatei)
//   /data/p<id>_fills.bin     letzte 100 Tankfüllungen (Ringdatei)
#pragma once
#include <cstdint>

#include "calc/persist.h"
#include "calc/trip.h"
#include "core/profile.h"
#include "core/ui_settings.h"

namespace store {

// LittleFS einbinden (bei Bedarf formatieren), Ordner anlegen. false = Dateisystem nicht nutzbar.
bool begin();

// Alle Profile als Kurzfassung; liefert die Anzahl
int listProfiles(ProfileSummary* out, int max);
bool loadProfile(uint8_t id, Profile& p);
// Speichert das Profil; id 0 bekommt die nächste freie Nummer. false = kein Platz bzw. Fehler.
bool saveProfile(Profile& p);

bool loadState(uint8_t id, PersistState& st);
bool saveState(const PersistState& st);

bool appendTrip(uint8_t id, const trip::TripRecord& r);
bool appendFill(uint8_t id, const trip::FillRecord& r);
// Fahrtenbuch bzw. Tankfüllungen lesen, ältester Eintrag zuerst; liefert die Anzahl
int readTrips(uint8_t id, trip::TripRecord* out, int max);
int readFills(uint8_t id, trip::FillRecord* out, int max);

// NVS: zuletzt benutztes Profil (0 = keins)
uint8_t lastProfileId();
void setLastProfileId(uint8_t id);

// NVS: Kacheln, Großanzeige, Diagramme. false = noch nichts gespeichert (Standard behalten)
bool loadUi(UiSettings& ui);
// NVS: gelernte Einbaulage des MPU6050 je Profil (A10): oben und vorne als Einheitsvektoren
bool loadImuAxes(uint8_t id, float axes[6]);
void saveImuAxes(uint8_t id, const float axes[6]);  // nullptr-artig: alle 0 = vergessen
void saveUi(const UiSettings& ui);

}  // namespace store
