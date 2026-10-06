#ifdef SIMULATE_OBD
#include "sim_task.h"

#include <Arduino.h>

#include "config.h"
#include "core/car_state_store.h"
#include "core/commands.h"
#include "sim/sim_link.h"
#include "sim/simulator.h"

namespace sim {

namespace {
// true, wenn die Klasse mit Takt periodMs seit der letzten Meldung wieder dran ist
bool due(uint32_t now, uint32_t& last, uint32_t periodMs) {
  if (last != 0 && now - last < periodMs) return false;
  last = now;
  return true;
}
}  // namespace

void task(void*) {
  DriveSim drive;
  carstate::modify([](CarState& s) { s.simulated = true; });

  // Verbindungsaufbau nachspielen (Startbildschirm), erst danach fließen Daten
  const uint32_t start = millis();
  for (;;) {
    const uint32_t elapsed = millis() - start;
    carstate::modify([&](CarState& s) { simlink::update(s.link, elapsed); });
    if (elapsed >= simlink::T_RUNNING) break;
    vTaskDelay(pdMS_TO_TICKS(cfg::SIM_STEP_MS));
  }
  Serial.println("Simulator: Fahrt startet (Kaltstart)");

  uint32_t lastMedium = 0, lastSlow = 0, lastRare = 0;
  bool engineWasOn = true;
  const float dtS = cfg::SIM_STEP_MS / 1000.0f;
  TickType_t wake = xTaskGetTickCount();

  for (;;) {
    Command c;
    while (commands::fromObd(c, 0)) {
      if (c.type == CmdType::SimSprint) drive.requestSprint();
      // Fehlercodes wie vom Auto: ein Steuergerät, gespeichert P0171 sobald gesetzt (A11)
      if (c.type == CmdType::ClearDtc && !drive.out().engineOn) {
        drive.clearDtc();
        Serial.println("Simulator: Fehlercodes gelöscht");
      }
      if (c.type == CmdType::ReadDtc || c.type == CmdType::ClearDtc) {
        const SimOutput& so = drive.out();
        const uint32_t t = millis();
        carstate::modify([&](CarState& s) {
          s.dtc.known = true;
          s.dtc.busy = false;
          s.dtc.failed = false;
          s.dtc.ecus = 1;
          s.dtc.nStored = so.dtc ? 1 : 0;
          s.dtc.stored[0] = so.dtc;
          s.dtc.nPending = 0;
          s.dtc.readAtMs = t;
          s.dtc.seq++;
          s.mil.set(so.mil ? 1.0f : 0.0f, t);
          s.dtcCount.set(so.dtcCount, t);
        });
      }
    }

    drive.step(dtS);
    const SimOutput& o = drive.out();
    if (o.engineOn != engineWasOn) {
      engineWasOn = o.engineOn;
      Serial.println(o.engineOn ? "Simulator: Motor an" : "Simulator: Motor aus (Tanken)");
    }

    const uint32_t now = millis();
    const bool medium = due(now, lastMedium, cfg::PID_PERIOD_MEDIUM_MS);
    const bool slow = due(now, lastSlow, cfg::PID_PERIOD_SLOW_MS);
    const bool rare = due(now, lastRare, cfg::PID_PERIOD_RARE_MS);
    carstate::modify([&](CarState& s) {
      // schnell: jede Runde
      s.speed.set(o.speedKmh, now);
      s.rpm.set(o.rpm, now);
      s.map.set(o.mapKpa, now);
      s.throttle.set(o.throttlePct, now);
      s.pedal.set(o.pedalPct, now);
      if (medium) {
        s.iat.set(o.iatC, now);
        s.stft.set(o.stftPct, now);
        s.ltft.set(o.ltftPct, now);
        s.fuelSys.set(o.fuelSys, now);
        s.load.set(o.loadPct, now);
      }
      if (slow) {
        s.coolant.set(o.coolantC, now);
        s.voltage.set(o.voltage, now);
        s.fuelLevel.set(o.fuelLevelPct, now);
      }
      if (rare) {
        s.mil.set(o.mil ? 1.0f : 0.0f, now);
        s.dtcCount.set(o.dtcCount, now);
      }
    });
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(cfg::SIM_STEP_MS));
  }
}

}  // namespace sim
#endif
