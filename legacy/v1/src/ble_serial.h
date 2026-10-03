#pragma once
#include <Arduino.h>

// Serielle Verbindung zu einem BLE-OBD-Adapter (ELM327 kompatibel), als Arduino-Stream,
// damit ELMduino sie wie eine normale serielle Schnittstelle benutzen kann.
//
// Funktioniert mit ESP32 und ESP32-S3. Der UART-Dienst des Adapters wird automatisch
// gesucht (z. B. FFF0/FFF1/FFF2 bei vLinker/Vgate, FFE0/FFE1 bei vielen Klonen).
class BleSerial : public Stream {
 public:
  void begin(const char* deviceName);
  // Sucht den Adapter per Name (oder MAC, falls nicht leer) und verbindet sich.
  // Blockiert für die Dauer des Scans (einige Sekunden).
  bool connect(const char* name, const char* mac);
  bool connected();
  void disconnect();

  int available() override;
  int read() override;
  int peek() override;
  size_t write(uint8_t c) override;
  size_t write(const uint8_t* buf, size_t len) override;
  void flush() override;
};
