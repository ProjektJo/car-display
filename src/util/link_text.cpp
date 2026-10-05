#include "link_text.h"

#include <cstdio>
#include <cstring>

#include "util/format.h"

namespace linktext {

const char* error(LinkError e) {
  switch (e) {
    case LinkError::AdapterNotFound: return "Kein Adapter gefunden";
    case LinkError::ConnectFailed: return "Adapter lässt sich nicht verbinden";
    case LinkError::NoUart: return "Adapter ohne Datendienst";
    case LinkError::AdapterSilent: return "Adapter antwortet nicht";
    case LinkError::NoVehicle: return "Auto antwortet nicht";
    case LinkError::ConnectionLost: return "Verbindung verloren";
    case LinkError::None: break;
  }
  return "";
}

const char* hint(LinkError e) {
  switch (e) {
    case LinkError::AdapterNotFound: return "Zündung an? Handy-App des Adapters schließen.";
    // ANNAHME: Der vLinker nimmt nur eine BLE-Verbindung an; meist hält eine Handy-App sie fest.
    case LinkError::ConnectFailed: return "Handy-App des Adapters schließen.";
    case LinkError::NoUart: return "Nur Adapter mit BT 4.0 (BLE) passen.";
    case LinkError::AdapterSilent: return "Adapter abziehen und wieder einstecken.";
    case LinkError::NoVehicle: return "Zündung an?";
    case LinkError::ConnectionLost: return "Adapter eingesteckt? Zündung an?";
    case LinkError::None: break;
  }
  return "";
}

void supported(const LinkInfo& li, char* out, size_t size) {
  if (!li.supportedKnown) {
    snprintf(out, size, "%s", fmt::NO_VALUE);
    return;
  }
  // Wichtige Werte, deren Fehlen man wissen will (A7: Verbrauchsquelle, Tank, Gaspedal)
  struct Key {
    uint8_t pid;
    const char* name;
  };
  static const Key KEYS[] = {{0x0B, "Saugrohr"}, {0x10, "MAF"}, {0x5E, "5E"}, {0x2F, "Tank"}, {0x49, "Gaspedal"}};
  int count = 0;
  for (int pid = 1; pid <= 0xFF; pid++)
    if (pid % 0x20 != 0 && li.pidSupported(static_cast<uint8_t>(pid))) count++;

  size_t len = static_cast<size_t>(snprintf(out, size, "%d", count));
  bool first = true;
  for (const Key& k : KEYS) {
    if (li.pidSupported(k.pid) || len >= size) continue;
    len += static_cast<size_t>(snprintf(out + len, size - len, "%s%s", first ? " (ohne " : ", ", k.name));
    first = false;
  }
  if (!first && len < size) snprintf(out + len, size - len, ")");
}

const char* vin(const LinkInfo& li) {
  // VIN wird zusammen mit der PID-Liste übernommen (obd_task, sim_link)
  if (!li.supportedKnown) return fmt::NO_VALUE;
  return li.vin[0] ? li.vin : "nicht geliefert";
}

const char* fuelSource(const LinkInfo& li, bool diesel) {
  if (!li.supportedKnown) return fmt::NO_VALUE;
  if (li.pidSupported(0x5E)) return "Kraftstoffrate (5E)";
  if (diesel) return "nicht verfügbar";  // Luftmenge sagt beim mager laufenden Diesel nichts (A7)
  if (li.pidSupported(0x10)) return "Luftmasse (MAF)";
  if (li.pidSupported(0x0B) && li.pidSupported(0x0C)) return "Saugrohrdruck";
  return "nicht verfügbar";
}

void startSteps(const LinkInfo& li, bool askingVehicle, Step out[STEP_COUNT]) {
  for (int i = 0; i < STEP_COUNT; i++) out[i] = Step{};
  auto set = [&](int i, StepKind k, const char* text) {
    out[i].kind = k;
    snprintf(out[i].text, sizeof(out[i].text), "%s", text);
  };
  char buf[48];
  snprintf(buf, sizeof(buf), "Verbunden mit %s", li.adapter);
  const char* connected = buf;

  switch (li.state) {
    case LinkState::Off:
    case LinkState::Searching:
      set(0, StepKind::Current, "Suche Adapter \xE2\x80\xA6");
      break;
    case LinkState::Connecting: {
      char t[48];
      snprintf(t, sizeof(t), "Verbinde mit %s \xE2\x80\xA6", li.adapter);
      set(0, StepKind::Current, t);
      break;
    }
    case LinkState::InitAdapter:
      set(0, StepKind::Done, connected);
      set(1, StepKind::Current, "Suche Protokoll \xE2\x80\xA6");
      break;
    case LinkState::ReadVehicle:
    case LinkState::Running: {
      set(0, StepKind::Done, connected);
      char t[48];
      snprintf(t, sizeof(t), "Protokoll: %s", li.protocol);
      set(1, StepKind::Done, t);
      if (li.state == LinkState::Running && li.vehicle[0]) {
        snprintf(t, sizeof(t), "Fahrzeug: %s", li.vehicle);
        set(2, StepKind::Done, t);
      } else if (li.state == LinkState::Running && askingVehicle) {
        set(2, StepKind::Current, "Welches Fahrzeug?");
      } else {
        // Liest PID-Liste und VIN bzw. sucht das passende Profil
        set(2, StepKind::Current, "Lese Fahrzeugdaten \xE2\x80\xA6");
      }
      break;
    }
    case LinkState::Waiting:
      if (li.error == LinkError::AdapterSilent || li.error == LinkError::NoVehicle) {
        set(0, StepKind::Done, connected);
        set(1, StepKind::Failed, error(li.error));
      } else {
        set(0, StepKind::Failed, error(li.error));
      }
      break;
  }
}

}  // namespace linktext
