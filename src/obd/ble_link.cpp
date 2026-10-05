#ifndef SIMULATE_OBD
#include "ble_link.h"

#include <Arduino.h>
#include <NimBLEDevice.h>

#include <cctype>
#include <cstring>
#include <initializer_list>
#include <string>

#include "config.h"

namespace ble {

namespace {

NimBLEClient* client = nullptr;
NimBLERemoteCharacteristic* txChar = nullptr;  // wir schreiben hierhin
NimBLERemoteCharacteristic* rxChar = nullptr;  // Adapter sendet hierüber (Notify)

// Empfangspuffer: wird vom BLE-Task befüllt, von obdTask gelesen
constexpr size_t RX_SIZE = 1024;
uint8_t rxBuf[RX_SIZE];
volatile size_t rxHead = 0, rxTail = 0;
portMUX_TYPE rxMux = portMUX_INITIALIZER_UNLOCKED;

void onNotify(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
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

// Findet einen Dienst mit einer Notify- und einer Write-Characteristic
bool findUart() {
  txChar = rxChar = nullptr;
  auto* services = client->getServices(true);
  for (auto* svc : *services) {
    if (isStandardService(svc->getUUID())) continue;
    NimBLERemoteCharacteristic *rx = nullptr, *tx = nullptr;
    for (auto* ch : *svc->getCharacteristics(true)) {
      if (!rx && (ch->canNotify() || ch->canIndicate())) rx = ch;
      if (!tx && (ch->canWrite() || ch->canWriteNoResponse())) tx = ch;
    }
    if (rx && tx) {
      Serial.printf("BLE-UART: Dienst %s, RX %s, TX %s\n", svc->getUUID().toString().c_str(),
                    rx->getUUID().toString().c_str(), tx->getUUID().toString().c_str());
      rxChar = rx;
      txChar = tx;
      return rx->subscribe(rx->canNotify(), onNotify);
    }
  }
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
    if (!findUart()) {
      Serial.println("BLE: kein UART-Dienst gefunden");
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
  const bool withResponse = !txChar->canWriteNoResponse();
  const size_t len = strlen(text);
  for (size_t pos = 0; pos < len; pos += cfg::BLE_CHUNK_BYTES) {
    const size_t n = (len - pos < cfg::BLE_CHUNK_BYTES) ? len - pos : cfg::BLE_CHUNK_BYTES;
    txChar->writeValue(reinterpret_cast<const uint8_t*>(text + pos), n, withResponse);
  }
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

}  // namespace ble
#endif
