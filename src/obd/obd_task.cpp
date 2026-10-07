// obdTask (Core 0): BLE-Verbindung, ELM-Init, unterstützte PIDs, VIN, PID-Scheduler und
// Neuverbindung (A6 "Beim Verbinden", A7, M Etappe 2).
//
// Ablauf:
//   Suche Adapter -> Verbinde -> ATZ … ATAT2 -> 0100 (sucht das Protokoll) -> ATDPN
//   -> 0120, 0140 … -> 0902 (VIN) -> Abfragen nach Takt-Klassen
// Jeder Fehler führt nach einer Pause von 1, 2, 5, 10 s (danach immer 10 s) zum nächsten Versuch.
#ifndef SIMULATE_OBD
#include "obd_task.h"

#include <Arduino.h>

#include <cmath>
#include <cstring>

#include "config.h"
#include "core/car_state_store.h"
#include "core/commands.h"
#include "obd/ble_link.h"
#include "obd/dtc.h"
#include "obd/elm327.h"
#include "obd/elm_parser.h"
#include "obd/pid_scheduler.h"

namespace obd {

namespace {

constexpr size_t RETRY_STEPS = sizeof(cfg::RETRY_DELAYS_S) / sizeof(cfg::RETRY_DELAYS_S[0]);
constexpr int MAX_MESSAGES = 8;
constexpr int MAX_VALUES = 16;

char reply[512];
elmp::Message msgs[MAX_MESSAGES];
elmp::PidValue vals[MAX_VALUES];
PidScheduler scheduler;
size_t retryStep = 0;
bool wantDtcRead = true;   // nach dem Verbinden einmal lesen
bool wantDtcClear = false;

void setState(LinkState st) {
  carstate::modify([&](CarState& s) {
    s.link.state = st;
    s.link.error = LinkError::None;
    s.link.retryInS = 0;
    if (st != LinkState::Running) s.link.queriesPerS = NAN;
  });
}

// Befehle der UI abholen (zurzeit keine für obdTask) und den Sprint-Takt übernehmen: Während einer
// Sprintmessung fragt der Scheduler nur Tempo, Drehzahl und Gaspedal (A10)
void drainCommands() {
  Command c;
  while (commands::fromObd(c, 0)) {
    if (c.type == CmdType::ReadDtc) wantDtcRead = true;
    if (c.type == CmdType::ClearDtc) wantDtcClear = true;
  }
  bool sprint = false;
  {
    CarState& s = carstate::lock();
    sprint = s.sprint.active;
    carstate::unlock();
  }
  scheduler.setSprint(sprint);
}

// Fehler anzeigen und mit wachsender Pause auf den nächsten Versuch warten (A7)
void waitRetry(LinkError err) {
  const uint32_t delayS = cfg::RETRY_DELAYS_S[retryStep];
  if (retryStep + 1 < RETRY_STEPS) retryStep++;
  for (uint32_t left = delayS; left > 0; left--) {
    carstate::modify([&](CarState& s) {
      s.link.state = LinkState::Waiting;
      s.link.error = err;
      s.link.retryInS = static_cast<uint8_t>(left);
      s.link.queriesPerS = NAN;
    });
    drainCommands();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void onAdapterFound(const char* name) {
  carstate::modify([&](CarState& s) {
    s.link.state = LinkState::Connecting;
    s.link.error = LinkError::None;
    snprintf(s.link.adapter, sizeof(s.link.adapter), "%s", name);
  });
}

LinkError toError(ble::ConnectResult r) {
  switch (r) {
    case ble::ConnectResult::NotFound: return LinkError::AdapterNotFound;
    case ble::ConnectResult::ConnectFailed: return LinkError::ConnectFailed;
    case ble::ConnectResult::NoUart: return LinkError::NoUart;
    case ble::ConnectResult::Ok: break;
  }
  return LinkError::None;
}

// Schickt eine Abfrage und zerlegt die Antwort in Nachrichten (msgCount, nur bei Daten).
elmp::Reply query(const char* cmd, uint32_t timeoutMs, int& msgCount, bool& lost) {
  msgCount = 0;
  lost = false;
  const elm::Result r = elm::command(cmd, reply, sizeof(reply), timeoutMs);
  if (r == elm::Result::Lost) {
    lost = true;
    return elmp::Reply::Empty;
  }
  if (r == elm::Result::Timeout) return elmp::Reply::Empty;
  const elmp::Reply kind = elmp::classify(reply);
  if (kind == elmp::Reply::Data) msgCount = elmp::parseMessages(reply, msgs, MAX_MESSAGES);
  return kind;
}

// Liste der unterstützten PIDs lesen. Die erste Abfrage 0100 sucht zugleich das Protokoll.
LinkError readSupported(uint8_t bits[32]) {
  memset(bits, 0, 32);
  int n = 0;
  bool lost = false;
  const elmp::Reply kind = query("0100", cfg::ELM_SEARCH_TIMEOUT_MS, n, lost);
  if (lost) return LinkError::ConnectionLost;
  uint32_t mask = 0;
  if (kind != elmp::Reply::Data || !elmp::decodeSupported(msgs, n, 0x00, mask)) {
    Serial.printf("OBD: 0100 ohne Antwort (%s)\n", reply);
    return LinkError::NoVehicle;
  }
  elmp::markSupported(bits, 0x00, mask);

  // Gefundenes Protokoll erfragen (A6: automatisch mit ATSP0)
  char proto[16];
  int number = -1;
  if (elm::command("ATDPN", proto, sizeof(proto), cfg::ELM_AT_TIMEOUT_MS) == elm::Result::Ok)
    number = elmp::parseProtocolNumber(proto);
  carstate::modify([&](CarState& s) {
    s.link.state = LinkState::ReadVehicle;
    s.link.isCan = elmp::protocolIsCan(number);
    s.link.protocolId = static_cast<int8_t>(number);
    snprintf(s.link.protocol, sizeof(s.link.protocol), "%s", elmp::protocolName(number));
  });
  Serial.printf("OBD: Protokoll %d (%s)\n", number, elmp::protocolName(number));

  // Weitere Listen 0120, 0140 …, solange das letzte Bit der vorigen Liste auf die nächste verweist
  for (int base = 0x20; base <= 0xE0; base += 0x20) {
    if (!(mask & 1u)) break;
    char cmd[8];
    snprintf(cmd, sizeof(cmd), "01%02X", base);
    const elmp::Reply k = query(cmd, cfg::ELM_DATA_TIMEOUT_MS, n, lost);
    if (lost) return LinkError::ConnectionLost;
    if (k != elmp::Reply::Data || !elmp::decodeSupported(msgs, n, static_cast<uint8_t>(base), mask)) break;
    elmp::markSupported(bits, static_cast<uint8_t>(base), mask);
  }
  return LinkError::None;
}

void logSupported(const uint8_t bits[32]) {
  Serial.printf("OBD: %d unterstützte PIDs:", elmp::countSupported(bits));
  for (int pid = 1; pid <= 0xFF; pid++)
    if ((bits[pid / 8] >> (pid % 8)) & 1) Serial.printf(" %02X", pid);
  Serial.println();
}

// VIN lesen (Mode 09 PID 02), falls das Auto sie liefert
void readVin(char* vin, size_t size) {
  vin[0] = '\0';
  int n = 0;
  bool lost = false;
  if (query("0902", cfg::ELM_VIN_TIMEOUT_MS, n, lost) != elmp::Reply::Data) return;
  if (!elmp::decodeVin(msgs, n, vin, size)) vin[0] = '\0';
}

// Mode-01-Werte in den CarState schreiben
void apply(const elmp::PidValue* v, int count, uint32_t now) {
  carstate::modify([&](CarState& s) {
    for (int i = 0; i < count; i++) {
      const float x = v[i].value;
      switch (v[i].pid) {
        case 0x0D: s.speed.set(x, now); break;
        case 0x0C: s.rpm.set(x, now); break;
        case 0x0B: s.map.set(x, now); break;
        case 0x11: s.throttle.set(x, now); break;
        case 0x49: s.pedal.set(x, now); break;
        case 0x0F: s.iat.set(x, now); break;
        case 0x06: s.stft.set(x, now); break;
        case 0x07: s.ltft.set(x, now); break;
        case 0x03: s.fuelSys.set(x, now); break;
        case 0x04: s.load.set(x, now); break;
        case 0x10: s.maf.set(x, now); break;
        case 0x43: s.absLoad.set(x, now); break;
        case 0x5E: s.fuelRate.set(x, now); break;
        case 0x44: s.lambdaCmd.set(x, now); break;
        case 0x05: s.coolant.set(x, now); break;
        case 0x2F: s.fuelLevel.set(x, now); break;
        case 0x01:
          s.mil.set(x, now);
          s.dtcCount.set(v[i].value2, now);
          break;
        default: break;
      }
    }
  });
}

// Fehlercodes lesen: Mode 03 (gespeichert) und 07 (vorläufig) (A11). false = Verbindung weg.
bool readDtcs() {
  carstate::modify([](CarState& s) { s.dtc.busy = true; });
  dtc::Code stored[DtcInfo::MAX] = {}, pending[DtcInfo::MAX] = {};
  int nStored = 0, nPending = 0, ecus = 0;
  bool ok = true;
  for (uint8_t mode : {0x03, 0x07}) {
    char cmd[4];
    snprintf(cmd, sizeof(cmd), "%02X", mode);
    const elm::Result r = elm::command(cmd, reply, sizeof(reply), cfg::ELM_VIN_TIMEOUT_MS);
    if (r == elm::Result::Lost) return false;
    const elmp::Reply kind = r == elm::Result::Ok ? elmp::classify(reply) : elmp::Reply::Error;
    if (kind == elmp::Reply::Data) {
      const int n = elmp::parseMessages(reply, msgs, MAX_MESSAGES);
      if (mode == 0x03) ecus = n;
      if (mode == 0x03)
        nStored = dtc::decode(msgs, n, mode, stored, DtcInfo::MAX);
      else
        nPending = dtc::decode(msgs, n, mode, pending, DtcInfo::MAX);
    } else if (kind != elmp::Reply::NoData) {
      ok = false;  // "NO DATA" heißt: keine Codes; alles andere ist ein Fehler
    }
  }
  const uint32_t now = millis();
  carstate::modify([&](CarState& s) {
    s.dtc.busy = false;
    s.dtc.failed = !ok;
    if (ok) {
      s.dtc.known = true;
      s.dtc.nStored = static_cast<uint8_t>(nStored);
      s.dtc.nPending = static_cast<uint8_t>(nPending);
      memcpy(s.dtc.stored, stored, sizeof(stored));
      memcpy(s.dtc.pending, pending, sizeof(pending));
      s.dtc.ecus = static_cast<uint8_t>(ecus);
      s.dtc.readAtMs = now;
    }
    s.dtc.seq++;
  });
  Serial.printf("Fehlercodes: %d gespeichert, %d vorläufig%s\n", nStored, nPending, ok ? "" : " (Abfrage fehlgeschlagen)");
  return true;
}

// Fehlercodes löschen (Mode 04), nur bei stehendem Motor (A11). false = Verbindung weg.
bool clearDtcs() {
  bool engineOff = false;
  {
    CarState& s = carstate::lock();
    const float rpm = s.rpm.get(millis());
    engineOff = std::isnan(rpm) || rpm < 1.0f;
    carstate::unlock();
  }
  if (!engineOff) {
    Serial.println("Fehlercodes: Löschen nur bei stehendem Motor");
    return true;
  }
  const elm::Result r = elm::command("04", reply, sizeof(reply), cfg::ELM_VIN_TIMEOUT_MS);
  if (r == elm::Result::Lost) return false;
  Serial.printf("Fehlercodes gelöscht: %s\n", reply);
  return true;
}

// Abfragen nach Takt-Klassen, bis die Verbindung abreißt oder das Auto schweigt
LinkError runLoop() {
  uint8_t failed = 0;
  uint32_t windowStart = millis();
  uint32_t windowCount = 0;
  char cmd[24];

  for (;;) {
    drainCommands();
    if (wantDtcClear) {
      wantDtcClear = false;
      if (!clearDtcs()) return LinkError::ConnectionLost;
      wantDtcRead = true;
    }
    if (wantDtcRead) {
      wantDtcRead = false;
      if (!readDtcs()) return LinkError::ConnectionLost;
    }
    const ObdRequest rq = scheduler.next(millis());
    if (rq.voltage) {
      snprintf(cmd, sizeof(cmd), "ATRV");
    } else {
      int len = snprintf(cmd, sizeof(cmd), "01");
      for (uint8_t i = 0; i < rq.count; i++) len += snprintf(cmd + len, sizeof(cmd) - len, "%02X", rq.pids[i]);
    }

    const elm::Result r = elm::command(cmd, reply, sizeof(reply), cfg::ELM_DATA_TIMEOUT_MS);
    const uint32_t now = millis();
    if (r == elm::Result::Lost) return LinkError::ConnectionLost;

    bool answered = false;
    if (r == elm::Result::Ok && rq.voltage) {
      float volts = 0;
      if (elmp::parseVoltage(reply, volts)) {
        carstate::modify([&](CarState& s) { s.voltage.set(volts, now); });
        windowCount++;
      }
    } else if (r == elm::Result::Ok && elmp::classify(reply) == elmp::Reply::Data) {
      const int n = elmp::parseMessages(reply, msgs, MAX_MESSAGES);
      const int count = elmp::decodeMode01(msgs, n, vals, MAX_VALUES);
      if (count > 0) {
        apply(vals, count, now);
        answered = true;
        windowCount++;
      }
    }
    // ATRV beantwortet der Adapter auch ohne Auto, zählt also nicht als Lebenszeichen des Autos
    if (!rq.voltage) failed = answered ? 0 : static_cast<uint8_t>(failed + 1);
    if (failed >= cfg::OBD_MAX_FAILED_REQUESTS) return LinkError::NoVehicle;

    // Abfragen pro Sekunde für den Diagnose-Dialog (A7)
    if (now - windowStart >= cfg::OBD_RATE_WINDOW_MS) {
      const float rate = windowCount * 1000.0f / static_cast<float>(now - windowStart);
      carstate::modify([&](CarState& s) { s.link.queriesPerS = rate; });
      windowStart = now;
      windowCount = 0;
    }
  }
}

}  // namespace

void task(void*) {
  carstate::modify([](CarState& s) { s.simulated = false; });
  ble::begin();
  char name[sizeof(LinkInfo::adapter)] = "";
  char version[32];
  char vin[18];
  uint8_t bits[32];

  for (;;) {
    // 1. Adapter suchen und verbinden
    if (!ble::connected()) {
      setState(LinkState::Searching);
      Serial.println("BLE: Suche Adapter …");
      const ble::ConnectResult r = ble::connect(name, sizeof(name), onAdapterFound);
      if (r != ble::ConnectResult::Ok) {
        waitRetry(toError(r));
        continue;
      }
      Serial.printf("BLE: verbunden mit %s\n", name);
      carstate::modify([](CarState& s) { snprintf(s.link.channel, sizeof(s.link.channel), "%s", ble::channelInfo()); });
    }

    // 2. Adapter einstellen
    setState(LinkState::InitAdapter);
    if (!elm::init(version, sizeof(version))) {
      const bool lost = !ble::connected();
      if (!lost) ble::disconnect();  // nächster Versuch beginnt mit einer frischen Verbindung
      waitRetry(lost ? LinkError::ConnectionLost : LinkError::AdapterSilent);
      continue;
    }
    Serial.printf("ELM: %s\n", version);

    // 3. Protokoll und unterstützte PIDs, dann VIN
    const LinkError err = readSupported(bits);
    if (err != LinkError::None) {
      waitRetry(err);
      continue;
    }
    logSupported(bits);
    readVin(vin, sizeof(vin));
    if (vin[0]) Serial.printf("OBD: VIN %s\n", vin);

    // 4. Laufender Betrieb
    bool can = false;
    carstate::modify([&](CarState& s) {
      memcpy(s.link.supported, bits, sizeof(bits));
      s.link.supportedKnown = true;
      snprintf(s.link.vin, sizeof(s.link.vin), "%s", vin);
      s.link.state = LinkState::Running;
      s.link.error = LinkError::None;
      s.link.everRunning = true;
      s.link.identSeq++;  // storageTask sucht jetzt das passende Profil (A6)
      can = s.link.isCan;
    });
    retryStep = 0;
    scheduler.reset(bits, can, cfg::OBD_MAX_PIDS_PER_REQUEST);
    wantDtcRead = true;  // nach jedem Verbinden die Fehlercodes lesen
    Serial.printf("OBD: läuft, %d Werte im Plan%s\n", scheduler.itemCount(), can ? ", CAN mit mehreren PIDs je Anfrage" : "");

    const LinkError why = runLoop();
    Serial.println(why == LinkError::ConnectionLost ? "BLE: Verbindung verloren" : "OBD: Auto antwortet nicht mehr");
    waitRetry(why);
  }
}

}  // namespace obd
#endif
