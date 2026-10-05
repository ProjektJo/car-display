// Befehle von der UI an obdTask und calcTask über FreeRTOS-Queues (A5).
// Die UI ruft nie direkt Funktionen der Daten-Tasks auf.
#pragma once
#include <cstdint>

enum class CmdType : uint8_t {
  SimSprint,    // Simulator: Vollgas-Sequenz beim nächsten Halt starten
  Refuel,       // an calcTask: f = Liter, f2 = Preis €/l, i = 1 vollgetankt (Tank-Fenster ab Etappe 5)
  EndTrip,      // an calcTask: Fahrt beenden (Menü, Etappe 7)
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
