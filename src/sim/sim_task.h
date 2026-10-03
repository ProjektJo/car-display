// Ersetzt im Simulator die obdTask: spielt die simulierte Fahrt ab und schreibt die Werte
// im Takt der PID-Klassen (A7) in den CarState, so wie es der echte Scheduler tun wird.
#pragma once

namespace sim {

void task(void* arg);

}  // namespace sim
