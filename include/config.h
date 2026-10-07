// Zentrale Schwellen, Zeiten und Standardwerte der Firmware.
//
// Regel: keine magischen Zahlen im Code. Jeder Wert steht hier mit Kommentar und Fundstelle
// (A = architektur.md mit Kapitel, U = ui-entwurf.md, M = master-prompt.md).
// Reines C++ ohne Arduino, damit die Rechenmodule auch nativ (Unit-Tests) kompilieren.
// Board-Pins stehen nicht hier, sondern in board_fnk0104b.h.
#pragma once
#include <cstdint>
// 1 = Flush per DMA. Bleibt 0: Test am 6.10.2026 mit Arduino-Kern 2.0.17 und TFT_eSPI 2.5.43 (FSPI) ergab
// eine Absturzschleife (Panic) direkt nach "Display: DMA an". Ohne DMA ist das Bild sauber, und
// 320 x 240 bei 40 MHz reicht dafür gut.
#ifndef DISPLAY_USE_DMA
#define DISPLAY_USE_DMA 0
#endif

namespace cfg {

// ---------------------------------------------------------------------------
// Version
// ---------------------------------------------------------------------------
constexpr const char* FW_VERSION = "2.1.0";

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
// Kanal durchprobieren: nach dem Einschalten der Benachrichtigung kurz warten, dann ATZ mit Zeitlimit
constexpr uint32_t BLE_PROBE_SETTLE_MS = 200;
constexpr uint32_t BLE_PROBE_TIMEOUT_MS = 2500;

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
constexpr float CUT_PEDAL_MARGIN_PCT = 2.0f;      // Pedal gilt bis 2 Prozentpunkte über "leer" als losgelassen

// Selbstkalibrierung zwischen zwei Vollbetankungen (A7)
constexpr float CAL_MIN_KM = 150.0f;
constexpr float CAL_RATIO_MIN = 0.7f;
constexpr float CAL_RATIO_MAX = 1.3f;
constexpr float FUEL_CAL_MIN = 0.5f;  // 7.10.2026 erweitert: Abgleich mit dem Bordcomputer (Menü)
constexpr float FUEL_CAL_MAX = 1.5f;
constexpr float CAL_CAR_MIN_KM = 3.0f;  // Abgleich mit dem Bordcomputer erst ab 3 km Fahrt
constexpr float SPEED_FACTOR_MIN = 0.85f, SPEED_FACTOR_MAX = 1.15f;  // Tempo-Abgleich mit dem Navi (Menü)
constexpr float SPEED_CAL_MIN_KMH = 30.0f;

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
// ANNAHME: Vorschlag im Tank-Fenster, solange noch nie ein Preis eingegeben wurde: 1,79⁹ €/l
constexpr float PRICE_DEFAULT = 1.799f;
// Automatische Tankerkennung nur mit 0x2F (A7): beim Motorstart den Füllstand 10 s im Stand mitteln
// und mit dem Füllstand beim Abstellen vergleichen; Anstieg ≥ 8 % der Tankgröße = getankt.
constexpr uint32_t REFUEL_AVG_MS = 10000;
constexpr float REFUEL_MIN_RISE_PCT = 8.0f;
constexpr float REFUEL_FULL_PCT = 95.0f;          // darüber die berechneten Liter seit der letzten Vollbetankung
constexpr float TRIP_AVG_MIN_KM = 0.5f;           // Fahrt & Tank: Ø Fahrt erst ab 0,5 km (U Seite 6)
constexpr float TANK_GOAL_MIN_KM = 5.0f;          // "Ø / Ziel" erst ab 5 km seit dem Tanken (Vorschau)

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
// Gang und Schaltempfehlung (A6 Gänge, A9)
// ---------------------------------------------------------------------------
// Lernen: nur stabile Phasen (A6)
constexpr float GEAR_LEARN_MIN_KMH = 10.0f;
constexpr float GEAR_LEARN_MIN_RPM = 1100.0f;
constexpr float GEAR_STABLE_WINDOW_S = 1.5f;      // k ändert sich über 1,5 s ...
constexpr float GEAR_STABLE_MAX_CHANGE = 0.03f;   // ... um weniger als 3 %
// ANNAHME: Histogramm mit logarithmischen Klassen von je 1 % zwischen k = 3 und k = 60
// (km/h je 1000 U/min); deckt alle Gänge vom Kleinwagen bis zum langen 6. Gang ab.
constexpr float GEAR_K_MIN = 3.0f;
constexpr float GEAR_BIN_RATIO = 1.01f;
constexpr uint16_t GEAR_BINS = 302;               // ln(60/3) / ln(1,01) ≈ 301
// ANNAHME: Eine Häufung zählt als Gang ab 20 Proben (2 s stabile Fahrt) und mindestens 2 % aller Proben;
// zwei Gänge liegen mindestens 12 % auseinander (sonst ist es dieselbe Häufung).
constexpr uint16_t GEAR_PEAK_MIN_SAMPLES = 20;
constexpr float GEAR_PEAK_MIN_SHARE = 0.02f;
constexpr float GEAR_PEAK_MIN_SEPARATION = 1.12f;
constexpr uint32_t GEAR_LEARN_MIN_TOTAL = 300;    // erst ab 30 s stabiler Fahrt Gänge ins Profil schreiben
constexpr uint32_t GEAR_RECHECK_MS = 10000;       // Häufungen alle 10 s neu suchen
constexpr float GEAR_MATCH_TOL = 0.06f;           // Gang erkannt, wenn k höchstens 6 % abweicht (A6)
constexpr float GEAR_NEUTRAL_MIN_KMH = 15.0f;     // Leerlaufdrehzahl bei Fahrt > 15 km/h = "N" (A6)
// ANNAHME: Leerlauf = Drehzahl unter 1000 U/min (warm ca. 780, kalt bis ca. 1000)
constexpr float GEAR_IDLE_MAX_RPM = 1000.0f;
// ANNAHME: Ist der kleinste gelernte Wert größer als 10,5, fehlt der 1. Gang (er ist selten lange
// stabil); die Zählung beginnt dann bei 2.
constexpr float GEAR_FIRST_MAX_K = 10.5f;
// Fahrzeug-Prüfung (A6): nach mindestens 2 min stabiler Phasen höchstens 1/3 passend = falsches Auto
constexpr float GEAR_CHECK_MIN_STABLE_S = 120.0f;
constexpr float GEAR_CHECK_MAX_MATCH_SHARE = 1.0f / 3.0f;

// Schaltempfehlung (A9)
constexpr float SHIFT_MAX_PEDAL_PCT = 60.0f;      // Gaspedal unter 60 %
constexpr float SHIFT_NEXT_MIN_RPM_PETROL = 1300.0f;  // Drehzahl im nächsten Gang mindestens ...
constexpr float SHIFT_NEXT_MIN_RPM_DIESEL = 1200.0f;
constexpr uint32_t SHIFT_ARROW_DELAY_MS = 1000;   // Pfeil, sobald die Empfehlung 1 s anliegt (A9)
// ANNAHME: Gaspedal "geschlossen" = höchstens 3 Prozentpunkte über dem kleinsten Wert seit dem Laden
constexpr float PEDAL_CLOSED_MARGIN_PCT = 3.0f;

// ---------------------------------------------------------------------------
// Eco-Score, Bremsenergie, Leistung (A9, A10)
// ---------------------------------------------------------------------------
constexpr float SCORE_W_ROLL = 0.35f, SCORE_W_CALM = 0.25f, SCORE_W_EARLY = 0.20f, SCORE_W_BRAKE = 0.10f,
                SCORE_W_IDLE = 0.10f;
constexpr float SCORE_ROLL_FULL_SHARE = 0.25f;    // 25 % der Fahrzeit rollen = 100 Punkte
constexpr float SCORE_CALM_PER_PCT_S = 25.0f;     // 100 − 25 × (Pedaländerung %/s − 1)
constexpr float SCORE_EARLY_FACTOR = 400.0f;      // 100 − 400 × Anteil offene Schaltempfehlung
constexpr float SCORE_BRAKE_ZERO_L100 = 1.5f;     // 0 Punkte ab 1,5 l/100 km gebremst
constexpr float SCORE_IDLE_ZERO_SHARE = 0.30f;    // 0 Punkte ab 30 % Standzeit
constexpr float SCORE_GOOD = 80.0f;               // ab 80 gut
constexpr float SCORE_OK = 60.0f;                 // darunter Luft nach oben
constexpr float ROLL_MAX_DECEL_MS2 = 0.5f;        // Segeln zählt als Rollen ohne Verzögerung > 0,5 m/s² (A9)
// ANNAHME: Pedal wird für "ruhiges Gas" mit τ = 0,3 s geglättet (Vorschau: Faktor 0,25 je Schritt)
constexpr float PEDAL_SMOOTH_TAU_S = 0.3f;
// ANNAHME: Beschleunigung aus der Tempoänderung zwischen zwei Abfragen, geglättet mit τ = 0,5 s
constexpr float ACCEL_SMOOTH_TAU_S = 0.5f;
constexpr float MOVING_MIN_KMH = 3.0f;            // ANNAHME: "in Bewegung" ab 3 km/h (Vorschau)
constexpr float AIR_DENSITY = 1.2f;               // kg/m³ (A9, A10)
constexpr float ROLL_COEFF = 0.012f;              // c_r (A6)
constexpr float GRAVITY = 9.81f;
constexpr float ENGINE_EFFICIENCY = 0.25f;        // Bremsenergie in Liter: J ÷ (0,25 · Heizwert) (A9)
// Schub gespart = Schubzeit × gelernter Leerlaufverbrauch (warm, im Stand) (A9)
// ANNAHME: bis er gelernt ist, gilt 0,7 l/h (Vorschau); gelernt wird ab 70 °C mit τ = 60 s.
constexpr float IDLE_LPH_DEFAULT = 0.7f;
constexpr float IDLE_LEARN_MIN_COOLANT_C = 70.0f;
constexpr float IDLE_LEARN_TAU_S = 60.0f;

// ---------------------------------------------------------------------------
// Spartipps (A9 Regelwerk)
// ---------------------------------------------------------------------------
constexpr uint32_t TIP_SHOW_MS = 8000;            // 8 s sichtbar
constexpr uint32_t TIP_GAP_MS = 60000;            // mindestens 60 s zwischen zwei Tipps
constexpr uint32_t TIP_SAME_GAP_MS = 300000;      // derselbe Tipp frühestens nach 5 min
constexpr uint32_t TIP_TOUCH_QUIET_MS = 3000;     // keine Tipps 3 s nach einem Tippen
// Gang rein: ausgekuppelt, > 20 km/h und Verzögerung > 0,5 m/s² seit 2 s
constexpr float TIP_COAST_MIN_KMH = 20.0f;
constexpr float TIP_COAST_DECEL_MS2 = 0.5f;
constexpr uint32_t TIP_COAST_HOLD_MS = 2000;
constexpr float TIP_COAST_END_DECEL_MS2 = 0.3f;   // Anlass vorbei, wenn kaum noch verzögert wird (Vorschau)
// Sanfter Gas geben: Pedal > 70 % seit > 3 s bei > 30 km/h
constexpr float TIP_HARD_PEDAL_PCT = 70.0f;
constexpr uint32_t TIP_HARD_HOLD_MS = 3000;
constexpr float TIP_HARD_MIN_KMH = 30.0f;
// Früher vom Gas: Verzögerung > 2,5 m/s² innerhalb von 4 s nach Pedal > 30 %
constexpr float TIP_LATE_DECEL_MS2 = 2.5f;
constexpr float TIP_LATE_PEDAL_PCT = 30.0f;
constexpr uint32_t TIP_LATE_WINDOW_MS = 4000;
// Langer Stand: Motor läuft, Tempo 0 seit > 60 s, danach alle 30 s
constexpr uint32_t TIP_IDLE_AFTER_MS = 60000;
constexpr uint32_t TIP_IDLE_REPEAT_MS = 30000;
constexpr float TIP_STAND_MAX_KMH = 1.0f;
// Gleichmäßig Gas: Tempo ± 3 km/h, Pedal-Standardabweichung > 8 % über 10 s
constexpr float TIP_STEADY_SPEED_BAND_KMH = 3.0f;
constexpr float TIP_STEADY_PEDAL_SD = 8.0f;
constexpr uint32_t TIP_STEADY_WINDOW_MS = 10000;
// ANNAHME: "Tempo konstant" erst ab 30 km/h, sonst schlägt die Regel im Stop-and-go an
constexpr float TIP_STEADY_MIN_KMH = 30.0f;

// Sprint und Auto-Sprint (A10)
constexpr float SPRINT_STAND_KMH = 0.5f;          // darunter steht das Auto
constexpr uint32_t SPRINT_STAND_MIN_MS = 1000;    // mindestens 1 s gestanden ...
constexpr uint32_t SPRINT_ARM_AFTER_LEAVE_MS = 3000;  // ... und spätestens 3 s nach dem Anfahren ...
// Nach der ersten Testfahrt (6.10.2026) geändert: Mit "Gaspedal ≥ 85 % roh und ≥ 3000 U/min" wurde kein
// Sprint erkannt. Viele Autos melden bei Vollgas nur 70–80 % Pedal, Diesel/Automatik drehen beim Anfahren
// keine 3000 U/min. Jetzt: Pedal relativ zum gelernten Bereich ODER kräftige Beschleunigung, ohne Drehzahl.
constexpr float SPRINT_PEDAL_PCT = 80.0f;         // ... Gaspedal ≥ 80 % des Bereichs (leer bis Vollgas) ...
constexpr float SPRINT_PEDAL_SPAN_MIN = 55.0f;    // Vollgas mindestens "leer + 55 %", bis ein höherer Wert gesehen wurde
constexpr float SPRINT_LAUNCH_KMH_S = 7.2f;       // ... oder im Schnitt ≥ 7,2 km/h je s (2 m/s², Jo) ...
constexpr uint32_t SPRINT_LAUNCH_MIN_MS = 1000;   // ... gemessen frühestens 1 s nach dem Anfahren
constexpr float SPRINT_80_ACCEL_MS2 = 1.5f;       // 80–120 startet auch ohne Pedal ab 1,5 m/s² bei 80 km/h
constexpr float SPRINT_ABORT_PEDAL_PCT = 50.0f;   // Auto-Sprint: zurück, wenn das Gas 2 s unter 50 % liegt
constexpr float SPRINT_ABORT_DROP_KMH = 3.0f;     // Abbruch: Tempo fällt um mehr als 3 km/h ...
constexpr uint32_t SPRINT_MAX_MS = 30000;         // ... oder die Messung dauert länger als 30 s ...
constexpr float SPRINT_MIN_ACCEL_MS2 = 0.8f;      // ... oder im Verlauf weniger als 0,8 m/s² über ...
constexpr uint32_t SPRINT_ACCEL_WINDOW_MS = 3000; // ... die letzten 3 s (eine Schaltpause ist kürzer)
constexpr uint32_t AUTO_SPRINT_RETURN_DONE_MS = 4000;  // zurück 4 s nach dem Ziel ...
constexpr uint32_t AUTO_SPRINT_RETURN_LOW_MS = 2000;   // ... 2 s nachdem das Gas unter 50 % fällt ...
constexpr uint32_t AUTO_SPRINT_NOT_RUNNING_MS = 3000;  // ... wenn 3 s nach dem Wechsel keine Messung läuft ...
constexpr uint32_t AUTO_SPRINT_MAX_MS = 25000;         // ... spätestens nach 25 s
constexpr float LIVE_WINDOW_S = 30.0f;            // Sport: Live-Diagramm der letzten 30 s
constexpr uint16_t LIVE_ENTRIES = 240;            // 30 s × 8 Abfragen/s
constexpr float RPM_GAUGE_MAX = 6500.0f;          // Drehzahlbogen 0–6500 U/min (A10)
constexpr float RPM_GAUGE_WARN = 5800.0f;         // ab 5800 warn
constexpr float KW_TO_PS = 1.36f;

// Spar-Ziel (Z 13, A9): aus / auto / fest. Auto = Ø der letzten 5 Tankfüllungen (ersatzweise Gesamt)
// minus 0,5 l, wenn dieser über 5 l liegt, sonst minus 0,3 l; mindestens 3,0. Fest 3,0–7,0 in 0,1-Schritten.
constexpr float GOAL_AUTO_MINUS_HIGH = 0.5f;
constexpr float GOAL_AUTO_MINUS_LOW = 0.3f;
constexpr float GOAL_AUTO_THRESHOLD = 5.0f;
constexpr float GOAL_MIN = 3.0f;
constexpr float GOAL_FIX_MIN = 3.0f, GOAL_FIX_MAX = 7.0f, GOAL_FIX_STEP = 0.1f;
constexpr float GOAL_FIX_DEFAULT = 5.5f;

// Wartung (Z 9): Ölwechsel und Inspektion nach km; Hinweis in der Start-Karte unter 500 km
// ANNAHME: Standard-Intervalle Öl 15.000 km (Z 9), Inspektion 30.000 km
constexpr float OIL_INTERVAL_DEFAULT_KM = 15000.0f;
constexpr float INSP_INTERVAL_DEFAULT_KM = 30000.0f;
constexpr float MAINT_WARN_KM = 500.0f;

// Spartempo (Z 1): 11 Klassen 30–130 km/h; zählt nur im höchsten Gang, Tempo ± 3 km/h über 20 s und
// ruhigem Gaspedal (ohne MPU6050). Eine Klasse gilt ab 5 km.
constexpr int TEMPO_CLASSES = 11;
constexpr float TEMPO_FIRST_KMH = 30.0f, TEMPO_STEP_KMH = 10.0f;
constexpr float TEMPO_BAND_KMH = 3.0f;
constexpr uint32_t TEMPO_STEADY_MS = 20000;
// ANNAHME: "ruhiges Gaspedal" = Pedal bleibt in ± 5 Prozentpunkten
constexpr float TEMPO_PEDAL_BAND_PCT = 5.0f;
constexpr float TEMPO_MIN_KM = 5.0f;
// Tempo-Tipp (A9): > 115 km/h seit > 30 s, Ersparnis aus den Klassen 100 und 120
constexpr float TIP_TEMPO_KMH = 115.0f;
constexpr uint32_t TIP_TEMPO_HOLD_MS = 30000;

// ---------------------------------------------------------------------------
// Optionale Sensoren (A3, A10, Etappe 8)
// ---------------------------------------------------------------------------
constexpr uint32_t SENSOR_PERIOD_MS = 10;         // MPU6050 100-mal pro Sekunde (A10)
constexpr uint8_t SENSOR_TASK_PRIO = 3;           // sensorTask (A5)
constexpr uint32_t SENSOR_TASK_STACK = 6 * 1024;
constexpr uint32_t GPS_BAUD = 9600;
// Einbaulage lernen (A10): 3 s Stillstand = "oben", geradeaus beschleunigen = "vorne"
constexpr float IMU_LEARN_UP_S = 3.0f;
constexpr float IMU_STILL_GYRO_RAD_S = 0.03f;     // ANNAHME: still = Drehrate unter 1,7 °/s
constexpr float IMU_STRAIGHT_GYRO_RAD_S = 0.05f;  // ANNAHME: geradeaus = Drehrate unter 3 °/s
constexpr float IMU_LEARN_FWD_MIN_MS2 = 1.0f;     // ANNAHME: deutlich beschleunigen = über 1 m/s² laut OBD
constexpr float IMU_LEARN_FWD_S = 1.5f;
constexpr float IMU_BIAS_STAND_S = 3.0f;          // Gyro-Nullpunkt bei jedem Stillstand über 3 s (A10)
constexpr float IMU_SLOPE_TAU_S = 3.0f;           // ANNAHME: Steigung aus OBD-Vergleich mit τ = 3 s
constexpr float IMU_OVERTAKE_MS2 = 1.5f;          // Überholvorgang: Längsbeschleunigung hoch (A9 Sanfter Gas)
constexpr float SLOPE_UPHILL_PCT = 3.0f;          // ANNAHME: "am Berg" ab 3 % Steigung (kein "Sanfter Gas")
constexpr float TEMPO_MAX_SLOPE_PCT = 1.0f;       // Spartempo nur bei Steigung unter 1 % (Z 1)
constexpr float G_FORCE_RANGE = 0.6f;             // G-Kraft-Seite: Kreis bis 0,6 g (Vorschau)
// GPS: km-Faktor (A7 Genauigkeit). ANNAHME: nach 20 km mit gutem Empfang (≥ 5 Satelliten, > 30 km/h)
// wird der Faktor zur Hälfte nachgeführt, Grenzen 0,9–1,1.
constexpr float KMF_MIN_KM = 20.0f;
constexpr float KMF_MIN_SPEED_KMH = 30.0f;
constexpr uint8_t KMF_MIN_SATS = 5;
constexpr float KMF_MIN = 0.9f, KMF_MAX = 1.1f;
constexpr uint32_t TRIP_GPS_PAUSE_S = 300;        // mit GPS-Uhrzeit: Pause > 5 min = neue Fahrt (A8)
constexpr float SUN_TRANSITION_MIN = 15.0f;       // Tag/Nacht mit 15 min Übergang (A9 Sonnenstand)
// Thermostat-Check (A9, Z 10): Fahrzeit > 15 min, davon > 8 min über 50 km/h, Kühlmittel < 75 °C,
// Ansaugluft beim Start > −5 °C; höchstens alle 10 Starts
constexpr float THERMO_DRIVE_S = 15 * 60.0f;
constexpr float THERMO_FAST_S = 8 * 60.0f;
constexpr float THERMO_FAST_KMH = 50.0f;
constexpr float THERMO_COOLANT_C = 75.0f;
constexpr float THERMO_MIN_IAT_C = -5.0f;
constexpr uint16_t THERMO_EVERY_STARTS = 10;

// Start-Karte (U Start-Karte, Z 12): aus der letzten Fahrt, wenn ≥ 1 km; 6 s, Tippen oder > 5 km/h schließt
constexpr float START_CARD_MIN_KM = 1.0f;
constexpr uint32_t START_CARD_SHOW_MS = 6000;
constexpr float START_CARD_CLOSE_KMH = 5.0f;

// Eco-Kurve (U Seite 1)
constexpr float ECO_CURVE_TOP_L100 = 12.0f;       // Y-Achse 0–12, Werte darüber oben mit ↑
constexpr float COLOR_GOOD_BELOW = 0.95f;         // good unter 95 % des Bezugs (M Farbschwellen)
constexpr float COLOR_WARN_ABOVE = 1.10f;         // warn über 110 %
constexpr uint32_t CHART_MIN_REDRAW_MS = 200;     // Diagramme zeichnen höchstens mit 5 Hz (M)

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
constexpr uint8_t BRIGHT_NIGHT_DEFAULT = 25;      // Nacht 1–60 %, 5er Schritte, Standard 25 (A2 Nr. 5)
constexpr uint8_t BRIGHT_NIGHT_MIN = 1;           // unter BRIGHT_NIGHT_FINE_BELOW in 1er Schritten (Jos Wunsch)
constexpr uint8_t BRIGHT_NIGHT_MAX = 60;
constexpr uint8_t BRIGHT_NIGHT_STEP = 5;
constexpr uint8_t BRIGHT_NIGHT_FINE_BELOW = 5;    // 1, 2, 3, 4, 5, 10, 15 … (PWM 10 Bit: 1 % = Tastgrad 10/1023)

// ---------------------------------------------------------------------------
// Bedienung (U Bedienung; Gesten-Schwellen aus der Vorschau)
// ---------------------------------------------------------------------------
constexpr uint32_t LONG_PRESS_MS = 800;           // langes Drücken 0,8 s (M)
constexpr int16_t TAP_SLOP_PX = 12;               // mehr Bewegung = kein Tippen und kein langes Drücken (Vorschau)
constexpr int16_t SWIPE_MIN_DX_PX = 40;           // Wischen links/rechts ab 40 px, waagerecht überwiegend (Vorschau)
constexpr int16_t SWIPE_DOWN_MIN_DY_PX = 50;      // Wischen von oben nach unten ab 50 px ... (Vorschau)
constexpr int16_t SWIPE_DOWN_START_MAX_Y_PX = 40; // ... wenn es in den obersten 40 px beginnt (Vorschau)
// Fingerausgleich (Jos Wunsch): Man tippt eher einen Tick unter den Knopf, um zu sehen, was man tippt.
// Der Touch-Punkt wird deshalb um so viele Pixel nach oben (aus Sicht des Fahrers) geschoben, in beiden
// Lagen gleich, außer im Touch-Test (der zeigt die reine Geometrie). Zu viel oder zu wenig: nur diese Zahl.
constexpr int16_t TOUCH_FINGER_OFFSET_Y_PX = 6;
constexpr bool TOUCH_DEBUG_LOG = true;            // Roh- und umgerechnete Touch-Koordinaten im seriellen Monitor
// Fehlersuche: Ein "S" über den seriellen Monitor schickt ein Bildschirmfoto an den PC (tools/screenshot.py)
constexpr bool SCREENSHOT_SERIAL = true;

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
