// Gemeinsamer I2C-Bus (A3): Touch (uiTask) und MPU6050 (sensorTask) teilen sich Wire.
// Jeder Zugriff vom Schreiben des Registers bis zum Lesen der Antwort läuft unter dieser Sperre.
#pragma once

namespace i2cbus {

void init();  // vor dem Start der Tasks
void lock();
void unlock();

// RAII-Hilfe: sperrt bis zum Ende des Blocks
struct Guard {
  Guard() { lock(); }
  ~Guard() { unlock(); }
};

}  // namespace i2cbus
