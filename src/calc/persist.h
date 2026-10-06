// Laufende Summen eines Profils, die Stromausfall und Abstellen überleben (A7 Mittelwerte, A8):
// Ringpuffer, Gesamtsummen, Tankmodell, Kalibrierung, laufende Fahrt, letzter Füllstand.
// Wird alle 60 s und bei jedem Stillstand über 10 s abwechselnd in zwei Dateien (A/B) mit
// Prüfsumme geschrieben. POD, reines C++.
#pragma once
#include <cstdint>
#include <cstring>

#include "calc/averages.h"
#include "calc/eco.h"
#include "calc/perf.h"
#include "calc/trip.h"
#include "config.h"

constexpr uint32_t PERSIST_MAGIC = 0x33534443;  // "CDS3"
// Version 2 (Etappe 4): Gang-Histogramm, Leerlaufverbrauch, letzte Fahrt, Eco-Summen der Fahrt
// Version 3 (Etappe 6): beste Sprintzeiten und -kurve, Vmax und Spitzenleistung der Fahrt
// Version 4 (Etappe 7): Spar-Ziel auto, Wartung, Spartempo
constexpr uint16_t PERSIST_VERSION = 4;

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

  eco::GearHistogram gearHist; // k-Werte stabiler Phasen (A6 Gänge lernen)
  float idleLph;               // gelernter Leerlaufverbrauch warm (Schub gespart, A9), NAN = noch nicht
  uint8_t hasLastTrip;         // 1 = lastTrip gültig (Start-Karte)
  uint8_t reserved2[3];
  trip::TripRecord lastTrip;   // zuletzt beendete Fahrt
  perf::Best sprintBest;       // beste Zeiten 0–50, 0–100, 80–120 und Kurve 0–100 (A10)
  float autoGoal;              // Spar-Ziel "auto", beim Tanken neu gesetzt (Z 13), NAN = noch keins
  // Wartung (Z 9): Tachostand = totalKm + odoOffsetKm; fällig bei Tachostand …Due
  float odoOffsetKm;           // NAN = Tachostand nie eingetragen
  float oilDueKm, oilIntervalKm;
  float inspDueKm, inspIntervalKm;
  // Spartempo (Z 1): km und Liter je Tempoklasse 30, 40 … 130 km/h
  float tempoKm[cfg::TEMPO_CLASSES];
  float tempoL[cfg::TEMPO_CLASSES];

  float spare[8];
  uint32_t crc;                // CRC-32 über alles davor
};

// Leerer Zustand für ein neues Profil
void initPersist(PersistState& st, uint8_t profileId);
