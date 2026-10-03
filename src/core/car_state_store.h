// Gemeinsamer CarState mit Mutex (FreeRTOS). Schreiben: obdTask/Simulator und calcTask.
// Lesen: uiTask per snapshot(), 10-mal pro Sekunde.
#pragma once
#include "car_state.h"

namespace carstate {

void init();

// Sperrt den Zustand für einen kurzen Schreib- oder Lesezugriff. Immer paarweise mit unlock().
CarState& lock();
void unlock();

// Ändert den Zustand unter dem Mutex: carstate::modify([&](CarState& s) { s.speed.set(...); });
template <class F>
void modify(F&& fn) {
  fn(lock());
  unlock();
}

// Kopiert den Zustand für die UI
void snapshot(CarSnapshot& out);

}  // namespace carstate
