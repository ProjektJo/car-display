// Zentrale Schwellen, Zeiten und Standardwerte der Firmware.
//
// Regel: keine magischen Zahlen im Code. Jeder Wert steht hier mit Kommentar und Fundstelle
// (A = architektur.md mit Kapitel, U = ui-entwurf.md, M = master-prompt.md).
// Reines C++ ohne Arduino, damit die Rechenmodule auch nativ (Unit-Tests) kompilieren.
// Board-Pins stehen nicht hier, sondern in board_fnk0104b.h.
#pragma once
#include <cstdint>

// 1 = Flush per DMA. Standard 0, bis DMA auf dem Board bestätigt ist: Beim ersten Test blieb das
// Bild schwarz, und TFT_eSPI-DMA zusammen mit USE_HSPI_PORT auf dem ESP32-S3 ist ein Verdacht.
// Ohne DMA zeichnet die Firmware wie die alte Firmware; 320 x 240 bei 40 MHz reicht dafür gut.
#ifndef DISPLAY_USE_DMA
#define DISPLAY_USE_DMA 0
#endif

namespace cfg {

// ---------------------------------------------------------------------------
// Version
// ---------------------------------------------------------------------------
constexpr const char* FW_VERSION = "2.0.1-etappe3";

// ---------------------------------------------------------------------------
// Aufgaben und Takt (A5)
// ---------------------------------------------------------------------------
constexpr uint32_t UI_SNAPSHOT_PERIOD_MS = 100;   // UI holt 10x/s eine Kopie des CarState (A5)
constexpr uint32_t UI_MAX_FPS = 30;               // höchstens 30 fps; in lv_conf.h als LV_DEF_REFR_PERIOD 33 (M)
constexpr uint32_t UI_LOOP_MAX_SLEEP_MS = 5;      // uiTask schläft höchstens so lange zwischen zwei LVGL-Durchläufen
constexpr uint32_t CALC_PERIOD_MS = 100;          // calcTask rechnet mit 10 Hz (A5)
constexpr uint32_t SERIAL_WAIT_MS = 1500;         // beim Start höchstens so lange auf den USB-Monitor warten (Fehlersuche)

// Stacks (Bytes) und Prioritäten der Tasks (A5)
constexpr uint32_t UI_TASK_STACK = 16 * 1024;
constexpr uint32_t OBD_TASK_STACK = 8 * 1024;
constexpr uint32_t CALC_TASK_STACK = 8 * 1024;
constexpr uint32_t STORAGE_TASK_STACK = 8 * 1024;  // LittleFS und ArduinoJson brauchen etwas Stack
constexpr uint8_t OBD_TASK_PRIO = 5;
constexpr uint8_t UI_TASK_PRIO = 4;
constexpr uint8_t CALC_TASK_PRIO = 4;
constexpr uint8_t STORAGE_TASK_PRIO = 1;
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
// Bluetooth und ELM327 (A6 "Beim Verbinden", A7 PID-Scheduler, M Verbindung)
// ---------------------------------------------------------------------------
// Name des Adapters (BLE). Leer = automatisch den ersten Adapter nehmen, dessen Name nach OBD aussieht
// (OBD, VLINK, VGATE, ELM, ICAR, V-LINK, KONNWEI). Beispiel: "vLinker MC-IOS".
constexpr const char* BLE_ADAPTER_NAME = "";
// Alternativ fest per MAC-Adresse, z. B. "00:10:cc:4f:36:03". Leer = per Name.
constexpr const char* BLE_ADAPTER_MAC = "";
constexpr const char* BLE_DEVICE_NAME = "Car-Display";  // so meldet sich das Display selbst
constexpr uint32_t BLE_SCAN_S = 5;                // Suche dauert 5 s (wie alte Firmware)
constexpr uint32_t BLE_CONNECT_TIMEOUT_S = 10;    // Verbindungsaufbau höchstens 10 s
constexpr uint32_t BLE_CHUNK_BYTES = 20;          // kleinste BLE-Nutzlast; längere Befehle in Stücken

// Neuer Versuch nach einem Fehler mit wachsender Pause 1, 2, 5, 10 s, danach immer 10 s (A7)
constexpr uint32_t RETRY_DELAYS_S[] = {1, 2, 5, 10};

// Antwortzeiten des Adapters (bis zum Prompt ">")
constexpr uint32_t ELM_RESET_TIMEOUT_MS = 3000;   // ATZ: Adapter startet neu
constexpr uint32_t ELM_AT_TIMEOUT_MS = 1500;      // übrige AT-Befehle
// ANNAHME: Die erste Abfrage nach ATSP0 sucht das Protokoll. KWP mit 5-Baud-Init braucht bis
// etwa 10 s, deshalb 15 s Geduld.
constexpr uint32_t ELM_SEARCH_TIMEOUT_MS = 15000;
constexpr uint32_t ELM_DATA_TIMEOUT_MS = 2000;    // normale Abfrage (ATAT2 kürzt selbst)
constexpr uint32_t ELM_VIN_TIMEOUT_MS = 5000;     // VIN kommt auf KWP in fünf Teilen
constexpr uint32_t ELM_POLL_MS = 2;               // so oft auf neue Bytes schauen
constexpr uint8_t OBD_MAX_PIDS_PER_REQUEST = 6;   // auf CAN bis zu 6 PIDs in einer Anfrage (A7, M)
// ANNAHME: Nach 5 Abfragen hintereinander ohne Antwort ist die Zündung aus bzw. das Auto weg.
// Dann wird das Protokoll neu gesucht (mit den Pausen von oben).
constexpr uint8_t OBD_MAX_FAILED_REQUESTS = 5;
constexpr uint32_t OBD_RATE_WINDOW_MS = 2000;     // "Abfragen pro Sekunde" über 2 s gemittelt (A7)
// ANNAHME: Der Startbildschirm bleibt nach "Fahrzeug: …" noch 1,5 s stehen, damit man ihn lesen kann.
constexpr uint32_t START_SCREEN_HOLD_MS = 1500;

// ---------------------------------------------------------------------------
// Fahrzeugprofile (A6)
// ---------------------------------------------------------------------------
// ANNAHME: höchstens 8 Profile; mehr Autos hat kaum jemand im Wechsel.
constexpr uint8_t MAX_PROFILES = 8;
// Vorbelegung des Assistenten "Neues Fahrzeug" (A6, Beispielprofil Renault Modus)
constexpr const char* DEFAULT_PROFILE_NAME = "Renault Modus";
constexpr const char* NEW_PROFILE_NAME = "Fahrzeug";     // ab dem zweiten Profil: "Fahrzeug 2" usw.
constexpr uint8_t PROFILE_NAME_MAX_CHARS = 16;            // ANNAHME: Namen bis 16 Zeichen
constexpr float DEFAULT_DISPLACEMENT_L = 1.2f;
constexpr float DEFAULT_TANK_L = 49.0f;
constexpr float DEFAULT_VE = 0.85f;                       // Füllungsgrad für Speed-Density (A7)
constexpr float DEFAULT_FUEL_CAL = 1.00f;
constexpr float DEFAULT_KM_FACTOR = 1.00f;
constexpr uint16_t DEFAULT_COLD_RPM_LIMIT = 2500;         // Kalt-Grenze (A9)
constexpr uint8_t DEFAULT_COLD_COOLANT_C = 60;            // kalt unter 60 °C (A9)
constexpr uint16_t DEFAULT_REDLINE_RPM = 6000;            // A6 Beispielprofil
constexpr uint16_t SHIFT_RPM_PETROL = 2200;               // Schaltempfehlung Benziner (A9)
constexpr uint16_t SHIFT_RPM_DIESEL = 1800;               // Diesel (A9)
// ANNAHME: Der Assistent fragt nicht nach der Leistung. Sie skaliert nur den Leistungsbalken (Etappe 6)
// und wird aus dem Hubraum geschätzt: 46 kW je Liter (Modus 1.2 16V: 55 kW).
constexpr float POWER_KW_PER_L = 46.0f;
// Eingabebereiche im Assistenten
constexpr float DISPLACEMENT_MIN_L = 0.6f, DISPLACEMENT_MAX_L = 6.0f, DISPLACEMENT_STEP_L = 0.1f;
constexpr float TANK_MIN_L = 20.0f, TANK_MAX_L = 120.0f, TANK_STEP_L = 1.0f;

// Fahrzeugart: Gewicht inkl. Fahrer und cw·A (A6 Tabelle). Index 0 = Standard (Kleinwagen).
struct BodyType {
  const char* key;   // im Profil-JSON
  const char* name;  // Anzeige
  float massKg;
  float cwA;         // m²
};
constexpr BodyType BODY_TYPES[] = {
    {"klein", "Kleinwagen", 1150.0f, 0.70f}, {"kompakt", "Kompakt", 1400.0f, 0.68f},
    {"limousine", "Limousine", 1550.0f, 0.62f}, {"kombi", "Kombi", 1600.0f, 0.70f},
    {"suv", "SUV", 1850.0f, 0.88f},           {"van", "Van", 1750.0f, 0.85f},
    {"transporter", "Transporter", 2300.0f, 1.15f},
};
constexpr uint8_t BODY_TYPE_COUNT = sizeof(BODY_TYPES) / sizeof(BODY_TYPES[0]);

// ---------------------------------------------------------------------------
// Verbrauch (A7 Verbrauchsberechnung)
// ---------------------------------------------------------------------------
constexpr float AFR_STOICH = 14.7f;               // Benzin; mit 0x44: AFR = 14,7 · λ_soll
constexpr float DENSITY_PETROL_G_PER_L = 745.0f;
constexpr float DENSITY_DIESEL_G_PER_L = 832.0f;
constexpr float HEAT_PETROL_MJ_PER_L = 32.0f;     // Heizwert (für Bremsenergie, Etappe 4)
constexpr float HEAT_DIESEL_MJ_PER_L = 36.0f;
constexpr float R_AIR_KJ_PER_KG_K = 0.28705f;     // Gaskonstante Luft
constexpr float KELVIN_OFFSET = 273.15f;
// ANNAHME: Liefert das Auto keine Ansauglufttemperatur (0x0F), rechnet Speed-Density mit 25 °C.
constexpr float IAT_FALLBACK_C = 25.0f;
constexpr float L100_MIN_SPEED_KMH = 5.0f;        // l/100 km erst ab 5 km/h, darunter l/h (A7)
constexpr uint32_t INSTANT_WINDOW_MS = 1000;      // Momentanverbrauch = 1-s-Mittel (A7)
// Schubabschaltung: 0x03 = 4 "open loop due to deceleration" (A7)
constexpr uint8_t FUEL_SYS_DECEL_CUT = 4;
// Ersatzregel ohne 0x03: Drosselklappe ≈ 0 % bei > 1200 U/min und > 15 km/h (A7).
// ANNAHME: "≈ 0 %" heißt höchstens 1,5 Prozentpunkte über dem kleinsten Wert seit dem Verbinden,
// weil viele Autos bei geschlossener Klappe nicht genau 0 % melden.
constexpr float CUT_FALLBACK_MIN_RPM = 1200.0f;
constexpr float CUT_FALLBACK_MIN_SPEED_KMH = 15.0f;
constexpr float CUT_THROTTLE_MARGIN_PCT = 1.5f;

// Selbstkalibrierung zwischen zwei Vollbetankungen (A7)
constexpr float CAL_MIN_KM = 150.0f;
constexpr float CAL_RATIO_MIN = 0.7f;
constexpr float CAL_RATIO_MAX = 1.3f;
constexpr float FUEL_CAL_MIN = 0.7f;
constexpr float FUEL_CAL_MAX = 1.3f;

// ---------------------------------------------------------------------------
// Mittelwerte über Strecke (A7)
// ---------------------------------------------------------------------------
constexpr uint16_t AVG1_SLOTS = 20;   constexpr float AVG1_SLOT_M = 50.0f;     // 1 km = 20 × 50 m
constexpr uint16_t AVG10_SLOTS = 100; constexpr float AVG10_SLOT_M = 100.0f;   // 10 km = 100 × 100 m
constexpr uint16_t AVG100_SLOTS = 100; constexpr float AVG100_SLOT_M = 1000.0f; // 100 km = 100 × 1 km
// ANNAHME: Ein Fenster zeigt einen Wert, sobald 10 % seiner Länge gefahren sind (1 km: 100 m,
// 10 km: 1 km, 100 km: 10 km). Vorher "–", damit die ersten Meter nicht wild springen.
constexpr float AVG_MIN_FRACTION = 0.1f;
constexpr float TANK_AVG_PREV_KM = 30.0f;         // erste 30 km nach dem Tanken: Schnitt der vorigen Füllung (A7)
// ANNAHME: Fahrt-, Tank- und Profil-Schnitt zeigen erst ab 1 km einen Wert.
constexpr float AVG_SIMPLE_MIN_KM = 1.0f;

// ---------------------------------------------------------------------------
// Tank, Preis, Reichweite (A7 Restreichweite, Tankfüllung)
// ---------------------------------------------------------------------------
constexpr float RANGE_W_100 = 0.5f;               // Prognose = 50 % Ø 100 km
constexpr float RANGE_W_10 = 0.3f;                //          + 30 % Ø 10 km
constexpr float RANGE_W_FILLS = 0.2f;             //          + 20 % Ø der letzten 5 Tankfüllungen
constexpr uint8_t RANGE_FILLS = 5;
constexpr float RANGE_TAU_S = 60.0f;              // nur die Prognose wird geglättet (τ = 60 s)
constexpr float RANGE_CAUTIOUS_KM = 80.0f;        // darunter mit dem höchsten der drei Schnitte
constexpr float RANGE_LOW_KM = 50.0f;             // darunter Tanksymbol und Kachel bernstein (A7, U)
// ANNAHME: Der Füllstand aus 0x2F schwappt; er wird mit τ = 30 s geglättet ("geglättet", A7).
constexpr float TANK_LEVEL_TAU_S = 30.0f;
constexpr float PRICE_TENTH_CENTS = 0.009f;       // feste ⁹ hinter Euro und Cent (A7)

// ---------------------------------------------------------------------------
// Speichern und Fahrtende (A8)
// ---------------------------------------------------------------------------
constexpr uint32_t SAVE_PERIOD_MS = 60000;        // laufende Summen alle 60 s ...
constexpr uint32_t SAVE_STANDSTILL_MS = 10000;    // ... und bei jedem Stillstand über 10 s
constexpr uint16_t TRIP_LOG_SIZE = 50;            // Fahrtenbuch: letzte 50 Fahrten (A2 Nr. 17)
constexpr uint16_t FILL_LOG_SIZE = 100;           // letzte 100 Tankfüllungen
constexpr float TRIP_WARM_C = 70.0f;              // Motor beim Abstellen warm ab 70 °C ...
constexpr float TRIP_PAUSE_MAX_DROP_C = 4.0f;     // ... und beim Start höchstens 4 °C kälter = kurze Pause
constexpr uint32_t TRIP_ENGINE_OFF_END_MS = 5UL * 60 * 1000;  // Strom bleibt an: 5 min ohne Motor = Fahrtende
// ANNAHME: Fahrten unter 100 m (z. B. nur Zündung an) kommen nicht ins Fahrtenbuch.
constexpr float TRIP_MIN_RECORD_KM = 0.1f;

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
