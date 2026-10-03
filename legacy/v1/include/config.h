#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Bluetooth LE / ELM327
// ---------------------------------------------------------------------------
// Bluetooth-LE-Name des OBD-Adapters, z. B. "vLinker MC-IOS", "OBDLink CX", "IOS-Vlink", "OBDII".
// Den genauen Namen zeigt der serielle Monitor beim Scan an ("gefunden: ...").
// Wir nutzen BLE (Bluetooth 4.0), weil der ESP32-S3 kein Bluetooth Classic kann.
// Leer = automatisch den ersten Adapter nehmen, dessen Name nach OBD aussieht.
#define ELM_BT_NAME   ""
// Alternativ per MAC verbinden. Leer lassen = per Name. Beispiel: "00:10:cc:4f:36:03"
#define ELM_BT_MAC    ""
// Name, unter dem sich der ESP32 meldet
#define DEVICE_BT_NAME "CarDisplay"

// ---------------------------------------------------------------------------
// Fahrzeug
// ---------------------------------------------------------------------------
// Für die Verbrauchsberechnung aus dem Luftmassenstrom (MAF), falls das
// Auto PID 0x5E (Fuel Rate) nicht liefert.
// Benzin: AFR 14.7, Dichte 745 g/l   |   Diesel: AFR 14.5, Dichte 832 g/l
// (Diesel läuft mager; MAF-Rechnung ist dort nur grob, PID 0x5E bevorzugen.)
constexpr float FUEL_AFR       = 14.7f;
constexpr float FUEL_DENSITY_G_PER_L = 745.0f;

// ---------------------------------------------------------------------------
// Warnschwellen (Wert wird rot angezeigt)
// ---------------------------------------------------------------------------
constexpr float WARN_COOLANT_C    = 105.0f;
constexpr float WARN_INTAKE_C     = 60.0f;
constexpr float WARN_VOLT_LOW     = 12.0f;  // Motor aus: < 12.0 V = Batterie schwach
constexpr float WARN_VOLT_CHARGE  = 13.2f;  // Motor an:  < 13.2 V = Lichtmaschine lädt nicht richtig
constexpr float WARN_VOLT_HIGH    = 15.0f;  // Überladung
constexpr float COLD_ENGINE_C     = 70.0f;  // darunter: "Motor kalt" (blau), nicht hochdrehen

// ---------------------------------------------------------------------------
// Diagramme
// ---------------------------------------------------------------------------
// Ein Punkt pro Intervall. 300 Punkte * 1 s = 5 Minuten Verlauf.
constexpr uint16_t HISTORY_LEN         = 300;
constexpr uint32_t HISTORY_INTERVAL_MS = 1000;

// ---------------------------------------------------------------------------
// Display / Touch (Freenove ESP32-S3 Display 2.8", FNK0104B)
// ---------------------------------------------------------------------------
// Display-Pins stehen in platformio.ini (TFT_eSPI). Touch: FT6336U über I2C.
constexpr int8_t TOUCH_SDA = 16;
constexpr int8_t TOUCH_SCL = 15;
constexpr int8_t TOUCH_RST = 18;
constexpr int8_t TOUCH_INT = 17;
constexpr uint8_t TOUCH_I2C_ADDR = 0x38;
// Helligkeit 0..255 (Backlight an GPIO 45 via PWM)
constexpr uint8_t BACKLIGHT = 255;
