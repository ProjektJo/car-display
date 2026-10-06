#ifndef SIMULATE_OBD
#include "ble_link.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>

#include <cctype>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "config.h"

namespace ble {

namespace {

NimBLEClient* client = nullptr;
NimBLERemoteCharacteristic* txChar = nullptr;  // wir schreiben hierhin
NimBLERemoteCharacteristic* rxChar = nullptr;  // Adapter sendet hierüber (Notify/Indicate)
bool txWithResponse = false;                   // gewählte Schreibart
char lastConn[64] = "";                        // Diagnose: gewählter Kanal

// Empfangspuffer: wird vom BLE-Task befüllt, von obdTask gelesen
constexpr size_t RX_SIZE = 1024;
uint8_t rxBuf[RX_SIZE];
volatile size_t rxHead = 0, rxTail = 0;
portMUX_TYPE rxMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool logRaw = false;  // beim Durchprobieren jeden Block als Hex ausgeben

void onNotify(NimBLERemoteCharacteristic* ch, uint8_t* data, size_t len, bool) {
  if (logRaw) {
    char hex[3 * 24 + 1];
    size_t n = 0;
    for (size_t i = 0; i < len && i < 24; i++) n += snprintf(hex + n, sizeof(hex) - n, "%02X ", data[i]);
    Serial.printf("BLE:   empfangen von %s, %u Byte: %s\n", ch->getUUID().toString().c_str(), (unsigned)len, hex);
  }
  portENTER_CRITICAL(&rxMux);
  for (size_t i = 0; i < len; i++) {
    const size_t next = (rxHead + 1) % RX_SIZE;
    if (next == rxTail) break;  // voll, Rest verwerfen
    rxBuf[rxHead] = data[i];
    rxHead = next;
  }
  portEXIT_CRITICAL(&rxMux);
}

bool isStandardService(const NimBLEUUID& u) {
  // Generic Access/Attribute, Device Information, Battery
  return u == NimBLEUUID((uint16_t)0x1800) || u == NimBLEUUID((uint16_t)0x1801) ||
         u == NimBLEUUID((uint16_t)0x180A) || u == NimBLEUUID((uint16_t)0x180F);
}

// Ein möglicher Kanal: Benachrichtigungs- und Schreib-Merkmal, Schreibart
struct Candidate {
  NimBLERemoteCharacteristic* rx;
  NimBLERemoteCharacteristic* tx;
  bool withResponse;
  int rank;            // bekannte Adapter zuerst
  bool indicate = false;  // Antworten per Indicate statt Notify
};

// Bekannte UART-Dienste von BLE-OBD-Adaptern (Dienst, Empfangen, Senden)
struct Known {
  const char* svc;
  const char* rx;
  const char* tx;
};
const Known KNOWN[] = {
    {"18f0", "2af0", "2af1"},  // Vgate "IOS-Vlink", iCar Pro BLE
    {"fff0", "fff1", "fff2"},  // vLinker, viele Klone
    {"ffe0", "ffe1", "ffe1"},  // HM-10-Klone (ein Merkmal für beides)
    {"e7810a71-73ae-499d-8c15-faa9aef0c3f2", "bef8d6c9-9c21-4c9e-b632-bd58c1009f9f",
     "bef8d6c9-9c21-4c9e-b632-bd58c1009f9f"},  // OBDLink CX/LX
};

std::string lower(std::string s) {
  for (auto& c : s) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return s;
}

bool uuidIs(const NimBLEUUID& u, const char* want) {
  const std::string s = lower(u.toString());
  const std::string w = lower(want);
  // 16-Bit-Kurzform "0x18f0" bzw. lange Form "000018f0-0000-1000-8000-00805f9b34fb"
  return s == w || s == "0x" + w || s.rfind("0000" + w + "-", 0) == 0;
}

int knownRank(NimBLERemoteService* svc, NimBLERemoteCharacteristic* rx, NimBLERemoteCharacteristic* tx) {
  for (int i = 0; i < static_cast<int>(sizeof(KNOWN) / sizeof(KNOWN[0])); i++)
    if (uuidIs(svc->getUUID(), KNOWN[i].svc) && uuidIs(rx->getUUID(), KNOWN[i].rx) && uuidIs(tx->getUUID(), KNOWN[i].tx)) return i;
  return 100;
}

// Wartet nach einem Befehl auf eine Antwort; true, wenn etwas Sinnvolles kam ("ELM", "OK" oder das Prompt ">")
bool waitReply(uint32_t timeoutMs) {
  std::string got;
  const uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    int c;
    while ((c = read()) >= 0) got += static_cast<char>(c);
    if (got.find('>') != std::string::npos || got.find("ELM") != std::string::npos || got.find("OK") != std::string::npos)
      return true;
    if (!connected()) return false;
    delay(20);
  }
  return false;
}

void sendRaw(NimBLERemoteCharacteristic* tx, const char* text, bool withResponse) {
  const size_t len = strlen(text);
  for (size_t pos = 0; pos < len; pos += cfg::BLE_CHUNK_BYTES) {
    const size_t n = (len - pos < cfg::BLE_CHUNK_BYTES) ? len - pos : cfg::BLE_CHUNK_BYTES;
    if (!tx->writeValue(reinterpret_cast<const uint8_t*>(text + pos), n, withResponse))
      Serial.printf("BLE:   Schreiben fehlgeschlagen (%s)\n", withResponse ? "mit Antwort" : "ohne Antwort");
  }
}

// Probiert einen Kanal: abonnieren, "\r", dann "ATZ\r" und auf eine Antwort warten
bool tryCandidate(const Candidate& c) {
  Serial.printf("BLE: probiere %s -> %s, %s, %s\n", c.rx->getUUID().toString().c_str(), c.tx->getUUID().toString().c_str(),
                c.withResponse ? "Schreiben mit Antwort" : "Schreiben ohne Antwort", c.indicate ? "Indicate" : "Notify");
  if (!c.rx->subscribe(!c.indicate, onNotify)) {
    Serial.println("BLE:   Benachrichtigung lässt sich nicht einschalten");
    return false;
  }
  delay(cfg::BLE_PROBE_SETTLE_MS);
  clearRx();
  sendRaw(c.tx, "\r", c.withResponse);
  delay(cfg::BLE_PROBE_SETTLE_MS);
  clearRx();
  sendRaw(c.tx, "ATZ\r", c.withResponse);
  if (waitReply(cfg::BLE_PROBE_TIMEOUT_MS)) return true;
  c.rx->unsubscribe();
  return false;
}

void remember(const std::string& addr, const Candidate& c, NimBLERemoteService* svc) {
  // ANNAHME: Diese kleine, seltene NVS-Schreibung macht obdTask selbst (nur beim ersten Verbinden je Adapter)
  Preferences p;
  if (!p.begin("cardisp_ble", false)) return;
  p.putString("addr", addr.c_str());
  p.putString("svc", svc->getUUID().toString().c_str());
  p.putString("rx", c.rx->getUUID().toString().c_str());
  p.putString("tx", c.tx->getUUID().toString().c_str());
  p.putBool("wr", c.withResponse);
  p.end();
}

// Sucht den UART-Kanal: zuerst der gemerkte, dann die bekannten Adapter, dann alle Paare aus
// Benachrichtigungs- und Schreib-Merkmal, jeweils mit beiden Schreibarten
bool findUart(const std::string& addr) {
  txChar = rxChar = nullptr;
  auto* services = client->getServices(true);
  std::vector<Candidate> cands;
  std::vector<NimBLERemoteService*> candSvc;
  for (auto* svc : *services) {
    if (isStandardService(svc->getUUID())) continue;
    auto* chars = svc->getCharacteristics(true);
    for (auto* ch : *chars)
      Serial.printf("BLE:   Dienst %s Merkmal %s (Handle %u):%s%s%s%s%s\n", svc->getUUID().toString().c_str(),
                    ch->getUUID().toString().c_str(), ch->getHandle(), ch->canRead() ? " lesen" : "",
                    ch->canWrite() ? " schreiben" : "", ch->canWriteNoResponse() ? " schreiben-ohne-Antwort" : "",
                    ch->canNotify() ? " notify" : "", ch->canIndicate() ? " indicate" : "");
    for (auto* rx : *chars) {
      if (!rx->canNotify() && !rx->canIndicate()) continue;
      for (auto* tx : *chars) {
        if (!tx->canWrite() && !tx->canWriteNoResponse()) continue;
        const int rank = knownRank(svc, rx, tx);
        for (bool ind : {false, true}) {
          if (!ind && !rx->canNotify()) continue;
          if (ind && !rx->canIndicate()) continue;
          for (bool wr : {false, true}) {
            if (wr && !tx->canWrite()) continue;
            if (!wr && !tx->canWriteNoResponse()) continue;
            Candidate c{rx, tx, wr, rank * 4 + (ind ? 2 : 0) + (wr ? 1 : 0)};
            c.indicate = ind;
            cands.push_back(c);
            candSvc.push_back(svc);
          }
        }
      }
    }
  }
  // Gemerkten Kanal dieses Adapters an den Anfang
  Preferences p;
  if (p.begin("cardisp_ble", true)) {
    if (p.getString("addr", "") == addr.c_str()) {
      const String rx = p.getString("rx", ""), tx = p.getString("tx", "");
      const bool wr = p.getBool("wr", false);
      for (auto& c : cands)
        if (c.rx->getUUID().toString() == rx.c_str() && c.tx->getUUID().toString() == tx.c_str() && c.withResponse == wr) c.rank = -1;
    }
    p.end();
  }
  // nach Rang sortieren (wenige Einträge)
  for (size_t i = 1; i < cands.size(); i++)
    for (size_t j = i; j > 0 && cands[j].rank < cands[j - 1].rank; j--) {
      std::swap(cands[j], cands[j - 1]);
      std::swap(candSvc[j], candSvc[j - 1]);
    }
  logRaw = true;
  // Zwei Durchgänge: erst ohne, dann mit gesicherter Verbindung (manche Adapter antworten nur gekoppelt)
  for (size_t i = 0; i < 2 * cands.size() && connected(); i++) {
    if (i == cands.size()) {
      Serial.println("BLE: keine Antwort, versuche gesicherte Verbindung (Kopplung)");
      NimBLEDevice::setSecurityAuth(true, false, true);
      if (!client->secureConnection()) Serial.println("BLE:   Kopplung fehlgeschlagen");
    }
    if (!tryCandidate(cands[i % cands.size()])) continue;
    i %= cands.size();
    logRaw = false;
    rxChar = cands[i].rx;
    txChar = cands[i].tx;
    txWithResponse = cands[i].withResponse;
    snprintf(lastConn, sizeof(lastConn), "%s/%s, %s", rxChar->getUUID().toString().c_str(), txChar->getUUID().toString().c_str(),
             txWithResponse ? "mit Antwort" : "ohne Antwort");
    Serial.printf("BLE: Kanal gefunden: %s\n", lastConn);
    if (cands[i].rank >= 0) remember(addr, cands[i], candSvc[i]);
    return true;
  }
  logRaw = false;
  return false;
}

bool sameMac(const std::string& a, const char* b) {
  if (a.size() != strlen(b)) return false;
  for (size_t i = 0; i < a.size(); i++)
    if (tolower(a[i]) != tolower(b[i])) return false;
  return true;
}

// Automatische Erkennung, wenn kein Name eingestellt ist
bool looksLikeObd(std::string n) {
  for (auto& c : n) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
  for (const char* key : {"OBD", "VLINK", "VGATE", "ELM", "ICAR", "V-LINK", "KONNWEI"})
    if (n.find(key) != std::string::npos) return true;
  return false;
}

}  // namespace

void begin() {
  NimBLEDevice::init(cfg::BLE_DEVICE_NAME);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
}

ConnectResult connect(char* foundName, size_t size, void (*onFound)(const char* name)) {
  const bool byMac = strlen(cfg::BLE_ADAPTER_MAC) > 0;
  const bool byName = strlen(cfg::BLE_ADAPTER_NAME) > 0;
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);
  NimBLEScanResults results = scan->start(cfg::BLE_SCAN_S, false);

  for (uint32_t i = 0; i < (uint32_t)results.getCount(); i++) {
    NimBLEAdvertisedDevice dev = results.getDevice(i);
    Serial.printf("  gefunden: %s  %s\n", dev.getAddress().toString().c_str(), dev.getName().c_str());
    const bool match = byMac    ? sameMac(dev.getAddress().toString(), cfg::BLE_ADAPTER_MAC)
                       : byName ? dev.getName() == cfg::BLE_ADAPTER_NAME
                                : looksLikeObd(dev.getName());
    if (!match) continue;

    snprintf(foundName, size, "%s", dev.getName().empty() ? dev.getAddress().toString().c_str() : dev.getName().c_str());
    if (onFound) onFound(foundName);
    scan->clearResults();
    if (!client) client = NimBLEDevice::createClient();
    client->setConnectTimeout(cfg::BLE_CONNECT_TIMEOUT_S);
    if (!client->connect(&dev)) {
      Serial.println("BLE: Verbindung fehlgeschlagen");
      return ConnectResult::ConnectFailed;
    }
    if (!findUart(dev.getAddress().toString())) {
      Serial.println("BLE: kein Kanal, auf dem der Adapter antwortet");
      client->disconnect();
      return ConnectResult::NoUart;
    }
    clearRx();
    return ConnectResult::Ok;
  }
  scan->clearResults();
  return ConnectResult::NotFound;
}

bool connected() { return client && client->isConnected(); }

void disconnect() {
  if (client) client->disconnect();
}

void write(const char* text) {
  if (!txChar || !connected()) return;
  sendRaw(txChar, text, txWithResponse);
}

int read() {
  int c = -1;
  portENTER_CRITICAL(&rxMux);
  if (rxHead != rxTail) {
    c = rxBuf[rxTail];
    rxTail = (rxTail + 1) % RX_SIZE;
  }
  portEXIT_CRITICAL(&rxMux);
  return c;
}

void clearRx() {
  portENTER_CRITICAL(&rxMux);
  rxTail = rxHead;
  portEXIT_CRITICAL(&rxMux);
}

const char* channelInfo() { return lastConn; }

}  // namespace ble
#endif
