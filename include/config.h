// Zentrale Schwellen, Zeiten und Standardwerte der Firmware.
//
// Regel: keine magischen Zahlen im Code. Jeder Wert steht hier mit Kommentar und Fundstelle
// (A = architektur.md mit Kapitel, U = ui-entwurf.md, M = master-prompt.md).
// Reines C++ ohne Arduino, damit die Rechenmodule auch nativ (Unit-Tests) kompilieren.
// Board-Pins stehen nicht hier, sondern in board_fnk0104b.h.
#pragma once
#include <cstdint>

// 1 = Flush per DMA. Bleibt das Bild schwarz oder zeigt es Streifen, hier 0 setzen (dann wie die alte Firmware ohne DMA).
#ifndef DISPLAY_USE_DMA
#define DISPLAY_USE_DMA 1
#endif

namespace cfg {

// ---------------------------------------------------------------------------
// Version
// ---------------------------------------------------------------------------
constexpr const char* FW_VERSION = "2.0.0-etappe1";

// ---------------------------------------------------------------------------
// Aufgaben und Takt (A5)
// ---------------------------------------------------------------------------
constexpr uint32_t UI_SNAPSHOT_PERIOD_MS = 100;   // UI holt 10x/s eine Kopie des CarState (A5)
constexpr uint32_t UI_MAX_FPS = 30;               // höchstens 30 fps; in lv_conf.h als LV_DEF_REFR_PERIOD 33 (M)
constexpr uint32_t UI_LOOP_MAX_SLEEP_MS = 5;      // uiTask schläft höchstens so lange zwischen zwei LVGL-Durchläufen
constexpr uint32_t CALC_PERIOD_MS = 100;          // calcTask rechnet mit 10 Hz (A5)

// Stacks (Bytes) und Prioritäten der Tasks (A5)
constexpr uint32_t UI_TASK_STACK = 16 * 1024;
constexpr uint32_t OBD_TASK_STACK = 8 * 1024;
constexpr uint32_t CALC_TASK_STACK = 6 * 1024;
constexpr uint8_t OBD_TASK_PRIO = 5;
constexpr uint8_t UI_TASK_PRIO = 4;
constexpr uint8_t CALC_TASK_PRIO = 4;
constexpr uint8_t CORE_DATA = 0;                  // obdTask, calcTask, sensorTask
constexpr uint8_t CORE_UI = 1;                    // uiTask, storageTask
constexpr uint8_t CMD_QUEUE_LEN = 8;              // Befehle UI -> obdTask bzw. calcTask

// ---------------------------------------------------------------------------
// Aktualität der Werte (M, Qualitätsregeln; A7 PID-Scheduler)
// ---------------------------------------------------------------------------
// "Werte älter als 3 s zeigt die UI als –". Die Klassen langsam (5 s) und selten (30 s)
// werden planmäßig seltener abgefragt.
// ANNAHME: Ein Wert gilt als veraltet, wenn seine nächste planmäßige Abfrage mehr als
// STALE_GRACE_MS überfällig ist. Für schnelle und mittlere Werte sind das rund 3 s,
// für langsame 8 s und für seltene 33 s. So flackert z. B. die Kühlmitteltemperatur
// nicht zwischen zwei Abfragen auf "–", und eingefrorene Zahlen gibt es trotzdem nie.
constexpr uint32_t STALE_GRACE_MS = 3000;
constexpr uint32_t PID_PERIOD_FAST_MS = 0;        // schnell: jede Runde (A7)
constexpr uint32_t PID_PERIOD_MEDIUM_MS = 1000;   // mittel: ca. 1 s (A7)
constexpr uint32_t PID_PERIOD_SLOW_MS = 5000;     // langsam: ca. 5 s (A7)
constexpr uint32_t PID_PERIOD_RARE_MS = 30000;    // selten: 30 s (A7)

// ANNAHME: Motor läuft, sobald die Drehzahl über diesem Wert liegt (Anlasser dreht ca. 200 U/min).
constexpr float ENGINE_RUNNING_MIN_RPM = 300.0f;

// ---------------------------------------------------------------------------
// Fahrzeug-Standardprofil (A6), bis die Profile in Etappe 3 kommen
// ---------------------------------------------------------------------------
constexpr float DEFAULT_TANK_L = 49.0f;           // Renault Modus
constexpr float RANGE_LOW_KM = 50.0f;             // Reichweite darunter: Tanksymbol und Kachel bernstein (A7, U)

// ---------------------------------------------------------------------------
// Display und Hintergrundlicht (A2 Nr. 5, M Hardware)
// ---------------------------------------------------------------------------
constexpr uint32_t DRAW_BUF_LINES = 40;           // Zeichenpuffer 2 x 320 x 40 Pixel, internes DMA-RAM (M)
constexpr uint32_t BACKLIGHT_PWM_FREQ_HZ = 5000;
constexpr uint8_t BACKLIGHT_PWM_BITS = 10;        // 10 Bit, damit auch 5 % noch fein abgestuft sind
constexpr uint8_t BACKLIGHT_PWM_CHANNEL = 7;      // LEDC-Kanal, sonst nirgends benutzt
constexpr uint8_t BRIGHT_DAY_DEFAULT = 80;        // Tag 10–100 %, 10er Schritte, Standard 80 (A2 Nr. 5)
constexpr uint8_t BRIGHT_DAY_MIN = 10;
constexpr uint8_t BRIGHT_DAY_MAX = 100;
constexpr uint8_t BRIGHT_DAY_STEP = 10;
constexpr uint8_t BRIGHT_NIGHT_DEFAULT = 25;      // Nacht 5–60 %, 5er Schritte, Standard 25 (A2 Nr. 5)
constexpr uint8_t BRIGHT_NIGHT_MIN = 5;
constexpr uint8_t BRIGHT_NIGHT_MAX = 60;
constexpr uint8_t BRIGHT_NIGHT_STEP = 5;

// ---------------------------------------------------------------------------
// Bedienung (U Bedienung; Gesten-Schwellen aus der Vorschau)
// ---------------------------------------------------------------------------
constexpr uint32_t LONG_PRESS_MS = 800;           // langes Drücken 0,8 s (M)
constexpr int16_t TAP_SLOP_PX = 12;               // mehr Bewegung = kein Tippen und kein langes Drücken (Vorschau)
constexpr int16_t SWIPE_MIN_DX_PX = 40;           // Wischen links/rechts ab 40 px, waagerecht überwiegend (Vorschau)
constexpr int16_t SWIPE_DOWN_MIN_DY_PX = 50;      // Wischen von oben nach unten ab 50 px ... (Vorschau)
constexpr int16_t SWIPE_DOWN_START_MAX_Y_PX = 40; // ... wenn es in den obersten 40 px beginnt (Vorschau)
constexpr bool TOUCH_DEBUG_LOG = true;            // Roh- und umgerechnete Touch-Koordinaten im seriellen Monitor

constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;       // BOOT-Taste entprellen
constexpr uint32_t BUTTON_LONG_MS = LONG_PRESS_MS;// BOOT lang = Menü (U)
// ANNAHME: Im Simulator startet ein sehr langer Druck (2 s) die Vollgas-Sequenz,
// ein langer Druck (0,8–2 s) öffnet wie im Auto das Menü.
constexpr uint32_t BUTTON_SIM_SPRINT_MS = 2000;

constexpr uint32_t OVERLAY_TIMEOUT_MS = 60000;    // Fenster schließen nach 60 s ohne Berührung, außer Tank-Fenster (M, U)
constexpr uint32_t STATUS_BLINK_MS = 500;         // Verbindungspunkt blinkt beim Verbinden (U)

// ---------------------------------------------------------------------------
// Simulator (M, Abschnitt Simulator)
// ---------------------------------------------------------------------------
constexpr uint32_t SIM_STEP_MS = 125;             // 8 Abfragen pro Sekunde wie ein schneller Adapter (A7: 4–8/s)
constexpr float SIM_AUTO_SPRINT_PERIOD_S = 240.0f;// Vollgas-Sequenz alle 4 min von selbst

}  // namespace cfg
