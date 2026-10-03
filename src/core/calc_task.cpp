#include "calc_task.h"

#include <Arduino.h>

#include "config.h"
#include "core/car_state_store.h"
#include "core/commands.h"

namespace calc {

void task(void*) {
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    Command c;
    while (commands::fromCalc(c, 0)) {
      // Befehle an die Rechnung kommen ab Etappe 3 (Fahrt beenden, Mittelwerte zurücksetzen ...)
    }
    carstate::modify([](CarState& s) {
      // Etappe 1: Liter im Tank direkt aus dem Füllstand (PID 0x2F) mal Tankgröße, mit dessen Zeitstempel.
      // Glättung, Tankmodell ohne 0x2F und Reichweite kommen in Etappe 3 (calc/fuel).
      if (s.fuelLevel.t != 0 && !std::isnan(s.fuelLevel.v)) {
        s.tankL.v = s.fuelLevel.v / 100.0f * cfg::DEFAULT_TANK_L;
        s.tankL.t = s.fuelLevel.t;
      }
    });
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(cfg::CALC_PERIOD_MS));
  }
}

}  // namespace calc
