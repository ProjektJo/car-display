// Simulator: spielt den Verbindungsaufbau nach, damit Startbildschirm und Diagnose-Dialog
// ohne Auto prüfbar sind (M Simulator). Reines C++, nativ testbar.
// Fahrzeug wie Renault Modus 1.2 vermutet (A7): KWP, Saugrohrdruck, kein MAF, kein 0x5E.
#pragma once
#include <cstdint>
#include <cstdio>

#include "config.h"
#include "core/car_state.h"

namespace simlink {

// Zeitpunkte ab Start in ms
constexpr uint32_t T_CONNECTING = 1500;  // Adapter gefunden
constexpr uint32_t T_INIT = 2500;        // BLE verbunden, ELM-Init und Protokollsuche
constexpr uint32_t T_READ = 4500;        // Protokoll gefunden, PIDs und VIN lesen
constexpr uint32_t T_RUNNING = 5500;     // Daten fließen

constexpr const char* ADAPTER = "vLinker MC-IOS";
constexpr const char* PROTOCOL = "ISO 14230-4 KWP";
// PIDs, die der Simulator liefert (simulator.h), dazu 0x20 als Verweis auf die zweite Liste
constexpr uint8_t PIDS[] = {0x01, 0x03, 0x04, 0x05, 0x06, 0x07, 0x0B, 0x0C, 0x0D, 0x0F, 0x11, 0x20, 0x2F, 0x40, 0x49};
constexpr float QUERIES_PER_S = 1000.0f / cfg::SIM_STEP_MS;

// Setzt den Verbindungszustand für die Zeit seit dem Start
inline void update(LinkInfo& li, uint32_t elapsedMs) {
  li.error = LinkError::None;
  li.retryInS = 0;
  snprintf(li.vehicle, sizeof(li.vehicle), "%s", cfg::DEFAULT_PROFILE_NAME);
  if (elapsedMs < T_CONNECTING) {
    li.state = LinkState::Searching;
    return;
  }
  snprintf(li.adapter, sizeof(li.adapter), "%s", ADAPTER);
  if (elapsedMs < T_INIT) {
    li.state = LinkState::Connecting;
    return;
  }
  if (elapsedMs < T_READ) {
    li.state = LinkState::InitAdapter;
    return;
  }
  snprintf(li.protocol, sizeof(li.protocol), "%s", PROTOCOL);
  li.isCan = false;
  if (elapsedMs < T_RUNNING) {
    li.state = LinkState::ReadVehicle;
    return;
  }
  if (!li.supportedKnown) {
    for (uint8_t pid : PIDS) li.supported[pid / 8] |= static_cast<uint8_t>(1u << (pid % 8));
    li.supportedKnown = true;
  }
  li.state = LinkState::Running;
  li.everRunning = true;
  li.queriesPerS = QUERIES_PER_S;
}

}  // namespace simlink
