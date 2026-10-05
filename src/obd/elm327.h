// ELM327-Client über die BLE-Verbindung (A12 obd/elm327). Eigener Client statt ELMduino (M Software-Stack).
// Befehl senden, Antwort bis zum Prompt ">" sammeln, mit Zeitlimit. Zerlegen der Antwort: elm_parser.h.
#pragma once
#include <cstddef>
#include <cstdint>

namespace elm {

enum class Result : uint8_t {
  Ok,        // Prompt erhalten, Antwort steht in out
  Timeout,   // kein Prompt innerhalb des Zeitlimits
  Lost,      // Bluetooth-Verbindung weg
};

// Schickt cmd (ohne "\r") und wartet höchstens timeoutMs auf das Prompt.
// Wartet dabei mit kurzen vTaskDelay, andere Tasks laufen weiter.
Result command(const char* cmd, char* out, size_t size, uint32_t timeoutMs);

// Init-Folge ATZ, ATE0, ATL0, ATS0, ATH0, ATSP0, ATAT2 (A7, M Verbindung).
// version bekommt die Antwort auf ATZ (z. B. "ELM327 v2.2"). false = Adapter antwortet nicht.
bool init(char* version, size_t size);

}  // namespace elm
