// Laufende Summen eines Profils, die Stromausfall und Abstellen überleben (A7 Mittelwerte, A8):
// Ringpuffer, Gesamtsummen, Tankmodell, Kalibrierung, laufende Fahrt, letzter Füllstand.
// Wird alle 60 s und bei jedem Stillstand über 10 s abwechselnd in zwei Dateien (A/B) mit
// Prüfsumme geschrieben. POD, reines C++.
#pragma once
#include <cstdint>
#include <cstring>

#include "calc/averages.h"
#include "calc/trip.h"

constexpr uint32_t PERSIST_MAGIC = 0x33534443;  // "CDS3"
constexpr uint16_t PERSIST_VERSION = 1;

struct PersistState {
  uint32_t magic;
  uint16_t version;
  uint8_t profileId;
  uint8_t reserved0;
  uint32_t seq;                // zählt jede Speicherung hoch; die höhere gültige Datei gewinnt

  avg::DistanceRing ring1;     // letzter Kilometer
  avg::DistanceRing ring10;    // 10 km
  avg::DistanceRing ring100;   // 100 km
  double totalKm;              // seit Profilanlage bzw. Reset
  double totalL;
  // Laufende Summen als double: Millionen kleiner Schritte würden in float merklich wegrunden
  double fillKm;               // seit der letzten Tankfüllung (berechnete Liter)
  double fillL;
  float prevFillL100;          // Ø der vorigen Füllung, NAN = keine
  avg::FillHistory fills;      // letzte 5 Füllungen für die Prognose

  uint8_t hadFullFill;         // schon einmal vollgetankt: Kalibrierzeitraum läuft
  uint8_t tankModelValid;
  uint8_t reserved1[2];
  double calFilledL;           // getankte Liter seit der letzten Vollbetankung
  double calComputedL;         // berechnete Liter im selben Zeitraum
  double calKm;
  double tankModelL;           // Tankmodell ohne 0x2F (A7)
  float mixPrice;              // Mischpreis im Tank €/l, NAN = unbekannt
  float pumpPrice;             // zuletzt eingegebener Zapfsäulenpreis, Vorschlag im Tank-Fenster
  float levelAtStopPct;        // letzter Füllstand (0x2F) für die Tankerkennung (Etappe 5)
  float coolantLastC;          // letzte Kühlmitteltemperatur: beim Start "beim Abstellen" (A8)

  trip::TripState trip;
  uint16_t lastTripNumber;
  uint16_t lastFillNumber;

  float spare[8];
  uint32_t crc;                // CRC-32 über alles davor
};

// Leerer Zustand für ein neues Profil
void initPersist(PersistState& st, uint8_t profileId);
