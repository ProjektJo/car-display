// Etappe 1: noch ohne Bluetooth. Die Task meldet "keine Verbindung", die UI zeigt den grauen
// Punkt und überall "–". BLE-Link, ELM327-Client und PID-Scheduler kommen in Etappe 2.
#ifndef SIMULATE_OBD
#include "obd_task.h"

#include <Arduino.h>

#include "core/car_state_store.h"
#include "core/commands.h"

namespace obd {

namespace {
constexpr uint32_t IDLE_WAIT_MS = 1000;
}

void task(void*) {
  carstate::modify([](CarState& s) {
    s.link = LinkState::Off;
    s.simulated = false;
  });
  Command c;
  for (;;) {
    commands::fromObd(c, IDLE_WAIT_MS);  // Befehle verwerfen, bis es etwas zu tun gibt
  }
}

}  // namespace obd
#endif
