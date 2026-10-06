// sensorTask (Core 0, A5): optionale Sensoren. MPU6050 am I2C-Bus (beim Start erkannt) und GPS an UART1
// (erkannt an gültigen NMEA-Sätzen). Ohne Sensor bleiben die zugehörigen Seiten und Menüpunkte ausgeblendet.
#pragma once

namespace sensors {

void task(void* arg);

// Menü → Diagnose → "Sensor neu einlernen" (uiTask)
void requestRelearn();

}  // namespace sensors
