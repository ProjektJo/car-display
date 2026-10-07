// Befehle von der UI an obdTask und calcTask über FreeRTOS-Queues (A5).
// Die UI ruft nie direkt Funktionen der Daten-Tasks auf.
#pragma once
#include <cstdint>

enum class CmdType : uint8_t {
  SimSprint,    // Simulator: Vollgas-Sequenz beim nächsten Halt starten
  Refuel,       // an calcTask: f = Liter, f2 = Preis €/l, i Bit 0 = vollgetankt, Bit 1–2 = trip::FillSource
  EndTrip,      // an calcTask: Fahrt beenden (Menü, Etappe 7)
  GearCheckYes, // an calcTask: "Fährst du mit Fahrzeug …?" mit Ja beantwortet: Gänge neu lernen
  GearCheckNo,  // an calcTask: Frage ohne Ja geschlossen bzw. Fahrzeug geändert
  ReadDtc,      // an obdTask: Fehlercodes lesen (Mode 03 und 07)
  ClearDtc,     // an obdTask: Fehlercodes löschen (Mode 04), nur bei stehendem Motor
  SetGoal,      // an calcTask: Spar-Ziel, i = 0 aus / 1 auto / 2 fest, f = fester Wert
  SetBody,      // an calcTask: Fahrzeugart, i = Index in cfg::BODY_TYPES
  SetColdRpm,   // an calcTask: Kalt-Grenze, i = U/min
  SetOdo,       // an calcTask: Tachostand eintragen, f = km
  MaintDone,    // an calcTask: Wartung erledigt, i = 0 Öl / 1 Inspektion
  SetInterval,  // an calcTask: Wartungsintervall, i = 0 Öl / 1 Inspektion, f = km
  ResetAvg,     // an calcTask: Mittelwerte zurücksetzen, i = Bitmaske (1 km, 10 km, 100 km, Tank)
  SetKmFactor,  // an calcTask: km-Faktor aus dem GPS-Vergleich, f = Faktor
  ImuRelearn,   // an sensorTask: Einbaulage neu einlernen
  // Weitere Befehle folgen mit den Etappen (Fehlercodes lesen/löschen ...)
};

struct Command {
  CmdType type;
  int32_t i = 0;
  float f = 0.0f;
  float f2 = 0.0f;
};

namespace commands {

void init();

// Senden aus der UI. Ist die Queue voll, geht der Befehl verloren (Rückgabe false).
bool toObd(const Command& c);
bool toCalc(const Command& c);

// Empfangen in den Daten-Tasks, wartet höchstens waitMs
bool fromObd(Command& c, uint32_t waitMs);
bool fromCalc(Command& c, uint32_t waitMs);

}  // namespace commands
