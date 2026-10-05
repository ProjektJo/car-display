// Bluetooth-LE-Verbindung zum OBD-Adapter (A12 obd/ble_link). Übernommen aus der getesteten
// alten Firmware (ble_serial.cpp): NimBLE, Suche nach Name bzw. MAC, automatische Suche des
// UART-Dienstes mit einem Schreib- und einem Benachrichtigungs-Merkmal
// (z. B. FFF0/FFF1/FFF2 bei vLinker/Vgate, FFE0/FFE1 bei vielen Klonen).
// Nur aus obdTask benutzen.
#pragma once
#include <cstddef>
#include <cstdint>

namespace ble {

enum class ConnectResult : uint8_t { Ok, NotFound, ConnectFailed, NoUart };

void begin();

// Sucht den Adapter (cfg::BLE_ADAPTER_MAC, sonst cfg::BLE_ADAPTER_NAME, sonst automatisch) und
// verbindet sich. Blockiert obdTask für die Suche (cfg::BLE_SCAN_S) und den Verbindungsaufbau.
// foundName bekommt den BLE-Namen des Adapters, sobald er gefunden ist; onFound wird dann gerufen
// (für den Startbildschirm "Verbinde mit …").
ConnectResult connect(char* foundName, size_t size, void (*onFound)(const char* name));

bool connected();
void disconnect();

// Schickt Text an den Adapter (in Stücken zu cfg::BLE_CHUNK_BYTES)
void write(const char* text);

// Nächstes empfangene Zeichen, -1 = nichts da
int read();

// Verwirft alles Empfangene
void clearRx();

}  // namespace ble
