#ifndef SIMULATE_OBD
#include "elm327.h"

#include <Arduino.h>

#include <cstring>
#include <initializer_list>

#include "config.h"
#include "obd/ble_link.h"
#include "obd/elm_parser.h"

namespace elm {

Result command(const char* cmd, char* out, size_t size, uint32_t timeoutMs) {
  size_t len = 0;
  if (size) out[0] = '\0';
  if (!ble::connected()) return Result::Lost;

  ble::clearRx();  // Reste einer abgebrochenen Antwort verwerfen
  char line[32];
  snprintf(line, sizeof(line), "%s\r", cmd);
  ble::write(line);

  const uint32_t start = millis();
  for (;;) {
    int c;
    while ((c = ble::read()) >= 0) {
      if (c == '>') {
        // Leerzeilen am Ende abschneiden
        while (len > 0 && (out[len - 1] == '\r' || out[len - 1] == '\n' || out[len - 1] == ' ')) out[--len] = '\0';
        return Result::Ok;
      }
      if (c == 0) continue;
      if (len + 1 < size) {
        out[len++] = static_cast<char>(c);
        out[len] = '\0';
      }
    }
    if (!ble::connected()) return Result::Lost;
    if (millis() - start >= timeoutMs) return Result::Timeout;
    vTaskDelay(pdMS_TO_TICKS(cfg::ELM_POLL_MS));
  }
}

bool init(char* version, size_t size) {
  char reply[64];
  // ATZ setzt den Adapter zurück; die Antwort ist die Versionskennung
  if (command("ATZ", reply, sizeof(reply), cfg::ELM_RESET_TIMEOUT_MS) != Result::Ok) return false;
  // Echo kann noch an sein: "ATZ" vor der Kennung abschneiden
  const char* v = strstr(reply, "ELM");
  if (!v) v = strstr(reply, "elm");
  snprintf(version, size, "%s", v ? v : reply);
  for (char* p = version; *p; p++)
    if (*p == '\r' || *p == '\n') *p = ' ';

  for (const char* cmd : {"ATE0", "ATL0", "ATS0", "ATH0", "ATSP0", "ATAT2"}) {
    if (command(cmd, reply, sizeof(reply), cfg::ELM_AT_TIMEOUT_MS) != Result::Ok) return false;
    // ANNAHME: Kennt ein Klon ATAT2 nicht ("?"), geht es ohne adaptives Timing weiter.
    if (elmp::classify(reply) == elmp::Reply::Error && strcmp(cmd, "ATAT2") != 0) {
      Serial.printf("ELM: %s abgelehnt (%s)\n", cmd, reply);
    }
  }
  return true;
}

}  // namespace elm
#endif
