#include "ble_serial.h"
#include <NimBLEDevice.h>

namespace {

NimBLEClient* client = nullptr;
NimBLERemoteCharacteristic* txChar = nullptr;  // wir schreiben hierhin
NimBLERemoteCharacteristic* rxChar = nullptr;  // Adapter sendet hierüber (Notify)

// Empfangspuffer: wird vom BLE-Task befüllt, von loop() gelesen
constexpr size_t RX_SIZE = 1024;
uint8_t rxBuf[RX_SIZE];
volatile size_t rxHead = 0, rxTail = 0;
portMUX_TYPE rxMux = portMUX_INITIALIZER_UNLOCKED;

// Sendepuffer: ELMduino schreibt zeichenweise, wir senden pro Befehl (bis '\r')
uint8_t txBuf[64];
size_t txLen = 0;

void onNotify(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
  portENTER_CRITICAL(&rxMux);
  for (size_t i = 0; i < len; i++) {
    size_t next = (rxHead + 1) % RX_SIZE;
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

bool sameMac(std::string a, const char* b) {
  if (a.size() != strlen(b)) return false;
  for (size_t i = 0; i < a.size(); i++)
    if (tolower(a[i]) != tolower(b[i])) return false;
  return true;
}

// Automatische Erkennung, wenn kein Name eingestellt ist
bool looksLikeObd(std::string n) {
  for (auto& c : n) c = toupper(c);
  for (const char* key : {"OBD", "VLINK", "VGATE", "ELM", "ICAR", "V-LINK", "KONNWEI"})
    if (n.find(key) != std::string::npos) return true;
  return false;
}

void sendTx() {
  if (!txLen) return;
  if (txChar && client && client->isConnected()) {
    bool withResponse = !txChar->canWriteNoResponse();
    txChar->writeValue(txBuf, txLen, withResponse);
  }
  txLen = 0;
}

}  // namespace

void BleSerial::begin(const char* deviceName) {
  NimBLEDevice::init(deviceName);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
}

bool BleSerial::connect(const char* name, const char* mac) {
  bool byMac = mac && strlen(mac);
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);
  NimBLEScanResults results = scan->start(5, false);

  for (uint32_t i = 0; i < (uint32_t)results.getCount(); i++) {
    NimBLEAdvertisedDevice dev = results.getDevice(i);
    Serial.printf("  gefunden: %s  %s\n", dev.getAddress().toString().c_str(), dev.getName().c_str());
    bool match = byMac ? sameMac(dev.getAddress().toString(), mac)
                 : (name && strlen(name)) ? dev.getName() == name
                 : looksLikeObd(dev.getName());
    if (match) {
      scan->clearResults();
      if (!client) client = NimBLEDevice::createClient();
      client->setConnectTimeout(10);
      if (!client->connect(&dev)) {
        Serial.println("BLE: Verbindung fehlgeschlagen");
        return false;
      }
      if (!findUart()) {
        Serial.println("BLE: kein UART-Dienst gefunden");
        client->disconnect();
        return false;
      }
      rxHead = rxTail = 0;
      return true;
    }
  }
  scan->clearResults();
  return false;
}

bool BleSerial::connected() { return client && client->isConnected(); }

void BleSerial::disconnect() {
  if (client) client->disconnect();
}

int BleSerial::available() {
  portENTER_CRITICAL(&rxMux);
  int n = (rxHead + RX_SIZE - rxTail) % RX_SIZE;
  portEXIT_CRITICAL(&rxMux);
  return n;
}

int BleSerial::read() {
  int c = -1;
  portENTER_CRITICAL(&rxMux);
  if (rxHead != rxTail) {
    c = rxBuf[rxTail];
    rxTail = (rxTail + 1) % RX_SIZE;
  }
  portEXIT_CRITICAL(&rxMux);
  return c;
}

int BleSerial::peek() {
  int c = -1;
  portENTER_CRITICAL(&rxMux);
  if (rxHead != rxTail) c = rxBuf[rxTail];
  portEXIT_CRITICAL(&rxMux);
  return c;
}

size_t BleSerial::write(uint8_t c) {
  txBuf[txLen++] = c;
  // Ein ELM-Befehl endet mit '\r'; volle Puffer (20 Byte = minimale BLE-Nutzlast) sofort senden
  if (c == '\r' || txLen >= 20) sendTx();
  return 1;
}

size_t BleSerial::write(const uint8_t* buf, size_t len) {
  for (size_t i = 0; i < len; i++) write(buf[i]);
  return len;
}

void BleSerial::flush() { sendTx(); }
