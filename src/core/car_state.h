// CarState: alle Fahrzeugwerte mit Zeitstempel je Wert (A5, M Qualitätsregeln).
//
// Datenfluss: obdTask (bzw. Simulator) und calcTask schreiben unter einem Mutex,
// uiTask holt sich 10-mal pro Sekunde eine Kopie (CarSnapshot). Nur uiTask ruft LVGL auf.
// Die Strukturen sind reines C++, damit Rechenmodule und Tests sie ohne Arduino nutzen können.
#pragma once
#include <cmath>
#include <cstdint>

#include "calc/perf.h"
#include "calc/trip.h"
#include "config.h"

// Ein Messwert mit Zeitstempel. Ohne gültige, aktuelle Messung liefert get() NAN,
// die UI zeigt dann "–" statt einer eingefrorenen Zahl.
struct Val {
  float v = NAN;
  uint32_t t = 0;          // Zeitpunkt der letzten Messung in ms (millis), 0 = noch nie
  uint32_t periodMs = 0;   // planmäßiger Abfragetakt (cfg::PID_PERIOD_*), bestimmt das Veralten

  void set(float value, uint32_t nowMs) {
    v = value;
    t = nowMs ? nowMs : 1;
  }
  void clear() {
    v = NAN;
    t = 0;
  }
  // Aktuell = gemessen und höchstens STALE_GRACE_MS über den nächsten planmäßigen Abfragezeitpunkt hinaus
  bool fresh(uint32_t nowMs) const {
    return t != 0 && !std::isnan(v) && (nowMs - t) <= periodMs + cfg::STALE_GRACE_MS;
  }
  float get(uint32_t nowMs) const { return fresh(nowMs) ? v : NAN; }
};

// Zustand der Verbindung zum Adapter (Startbildschirm und Verbindungspunkt, U Rahmen, U Startbildschirm)
enum class LinkState : uint8_t {
  Off,          // keine Verbindung, kein Versuch
  Searching,    // Suche Adapter
  Connecting,   // BLE-Verbindung wird aufgebaut
  InitAdapter,  // ELM-Init, Protokollsuche
  ReadVehicle,  // unterstützte PIDs und VIN lesen
  Running,      // Daten fließen
  Waiting,      // Fehler, warte auf den nächsten Versuch (linkError sagt warum)
};

// Grund des letzten Fehlers; die UI zeigt dazu einen Satz mit der Lösung (U Startbildschirm)
enum class LinkError : uint8_t {
  None,
  AdapterNotFound,  // kein BLE-Adapter in Reichweite
  ConnectFailed,    // Adapter gefunden, Verbindung klappt nicht (meist: Handy-App ist verbunden)
  NoUart,           // Adapter ohne Dienst mit Schreib- und Benachrichtigungs-Merkmal
  AdapterSilent,    // Adapter antwortet nicht auf ATZ
  NoVehicle,        // Adapter da, Auto antwortet nicht (Zündung aus)
  ConnectionLost,   // Bluetooth-Verbindung abgerissen
};

// Angaben zur Verbindung (Startbildschirm, Diagnose-Dialog). Texte sind kurz und fest begrenzt,
// damit die Kopie für die UI klein bleibt.
struct LinkInfo {
  LinkState state = LinkState::Off;
  LinkError error = LinkError::None;
  uint8_t retryInS = 0;        // Sekunden bis zum nächsten Versuch (bei Waiting)
  bool everRunning = false;    // seit dem Einschalten schon einmal Daten bekommen
  char adapter[24] = "";       // BLE-Name des Adapters
  char channel[48] = "";       // gewählter BLE-Kanal (Diagnose "Letzte Verbindung")
  char protocol[28] = "";      // z. B. "ISO 14230-4 KWP"
  int8_t protocolId = -1;      // ELM-Protokollnummer (ATDPN), -1 = unbekannt
  bool isCan = false;
  char vin[18] = "";           // leer = nicht gelesen bzw. nicht geliefert
  char vehicle[33] = "";       // Name des geladenen Profils
  bool supportedKnown = false;
  uint8_t supported[32] = {};  // Bitfeld Mode 01 PID 0x00–0xFF (Bit pid%8 in Byte pid/8)
  float queriesPerS = NAN;     // Abfragen pro Sekunde (A7), NAN ohne Verbindung
  uint16_t identSeq = 0;       // zählt jedes fertige Auslesen von PID-Liste und VIN hoch (Profil erkennen)

  bool pidSupported(uint8_t pid) const { return supportedKnown && (supported[pid / 8] >> (pid % 8)) & 1; }
};

// Geladenes Fahrzeugprofil (A6), für Statusleiste, Startbildschirm und Dialoge
struct ProfileInfo {
  uint8_t id = 0;              // 0 = keins geladen
  char name[33] = "";
  bool asking = false;         // "Welches Fahrzeug?" offen: kein Profil passt eindeutig
  uint16_t askSeq = 0;         // zählt jede neue Frage hoch (UI öffnet die Auswahl einmal je Frage)
  uint8_t count = 0;           // Anzahl gespeicherter Profile
  uint8_t body = 0;            // Fahrzeugart (Index in cfg::BODY_TYPES)
  bool diesel = false;
  float fuelCal = NAN;
  float kmFactor = 1.0f;       // Tempo und Strecke: echt = OBD · Faktor (GPS bzw. Tempo-Abgleich)
  float tankL = NAN;
  uint16_t coldRpmLimit = cfg::DEFAULT_COLD_RPM_LIMIT;  // Kalter Motor (A9)
  uint8_t coldCoolantC = cfg::DEFAULT_COLD_COOLANT_C;
  uint16_t powerKw = 55;            // Skala des Leistungsbalkens (A10)
  float displacementL = NAN;        // Hubraum (Menü "Fahrzeug")
  float reserveL = NAN;             // nicht nutzbare Reserve im Tank
  uint16_t redlineRpm = cfg::DEFAULT_REDLINE_RPM;
};

// Fehlercodes (A11): gespeichert (Mode 03) und vorläufig (Mode 07)
struct DtcInfo {
  static constexpr int MAX = 8;
  bool known = false;           // mindestens einmal gelesen
  bool busy = false;            // Lesen bzw. Löschen läuft
  bool failed = false;          // letzte Abfrage ohne Antwort
  uint8_t nStored = 0, nPending = 0;
  uint16_t stored[MAX] = {};
  uint16_t pending[MAX] = {};
  uint8_t ecus = 0;             // Zahl der antwortenden Steuergeräte
  uint32_t readAtMs = 0;        // Zeitpunkt des letzten Lesens
  uint16_t seq = 0;             // zählt jedes Lesen hoch
};

// Sprintmessung (A10) für Sport- und Sprint-Seite
struct SprintInfo {
  perf::State state = perf::State::Ready;
  bool active = false;          // Messung aktiv: Scheduler nur Tempo, Drehzahl, Gas
  bool run80 = false;
  float elapsed = NAN;          // laufende Zeit 0–100 s
  uint32_t doneAtMs = 0;
  uint16_t launchSeq = 0;       // Sprint erkannt (Auto-Sprint)
  float last50 = NAN, last100 = NAN, last80120 = NAN;
  float best50 = NAN, best100 = NAN, best80120 = NAN;
  perf::Trace lastTrace = {};
  perf::Trace bestTrace = {};
  bool standing = true;         // steht (Sprint-Seite: "READY")
  uint16_t resultSeq = 0;       // zählt bei jedem neuen Ergebnis hoch (Sport: Urteil)
  perf::Kind resultKind = perf::Kind::None;
  float resultS = NAN, resultPrevBest = NAN;
};

struct CarState {
  // --- Verbindung ---
  LinkInfo link;
  bool simulated = false;
  ProfileInfo profile;

  // --- Rohwerte OBD Mode 01 (PID in Klammern, Takt-Klasse nach A7) ---
  Val speed{NAN, 0, cfg::PID_PERIOD_FAST_MS};          // km/h (0x0D)
  Val rpm{NAN, 0, cfg::PID_PERIOD_FAST_MS};            // U/min (0x0C)
  Val map{NAN, 0, cfg::PID_PERIOD_FAST_MS};            // Saugrohrdruck kPa (0x0B)
  Val throttle{NAN, 0, cfg::PID_PERIOD_FAST_MS};       // Drosselklappe % (0x11)
  Val pedal{NAN, 0, cfg::PID_PERIOD_FAST_MS};          // Gaspedal % (0x49)
  Val iat{NAN, 0, cfg::PID_PERIOD_MEDIUM_MS};          // Ansaugluft °C (0x0F)
  Val stft{NAN, 0, cfg::PID_PERIOD_MEDIUM_MS};         // kurzfristige Gemischkorrektur % (0x06)
  Val ltft{NAN, 0, cfg::PID_PERIOD_MEDIUM_MS};         // langfristige Gemischkorrektur % (0x07)
  Val fuelSys{NAN, 0, cfg::PID_PERIOD_MEDIUM_MS};      // Kraftstoffsystem-Status (0x03), 4 = Schub
  Val load{NAN, 0, cfg::PID_PERIOD_MEDIUM_MS};         // Motorlast % (0x04)
  Val maf{NAN, 0, cfg::PID_PERIOD_MEDIUM_MS};          // Luftmasse g/s (0x10)
  Val absLoad{NAN, 0, cfg::PID_PERIOD_MEDIUM_MS};      // absolute Last % (0x43): Luft je Hub, vom Steuergerät
  Val o2V{NAN, 0, cfg::PID_PERIOD_MEDIUM_MS};          // Lambdasonde Spannung V (0x14–0x1B): im Schub fast 0
  Val fuelRate{NAN, 0, cfg::PID_PERIOD_FAST_MS};       // Kraftstoff l/h (0x5E)
  Val lambdaCmd{NAN, 0, cfg::PID_PERIOD_MEDIUM_MS};    // Soll-Lambda (0x44)
  Val coolant{NAN, 0, cfg::PID_PERIOD_SLOW_MS};        // Kühlmittel °C (0x05)
  Val voltage{NAN, 0, cfg::PID_PERIOD_SLOW_MS};        // Bordspannung V (ATRV)
  Val fuelLevel{NAN, 0, cfg::PID_PERIOD_SLOW_MS};      // Tankfüllstand % (0x2F)
  Val mil{NAN, 0, cfg::PID_PERIOD_RARE_MS};            // Motorkontrollleuchte 0/1 (0x01)
  Val dtcCount{NAN, 0, cfg::PID_PERIOD_RARE_MS};       // Anzahl gespeicherter Fehlercodes (0x01)

  // --- Abgeleitete Werte (calcTask, jede 100 ms; NAN = nicht berechenbar) ---
  Val fuelLph{NAN, 0, cfg::PID_PERIOD_FAST_MS};        // Momentanverbrauch l/h (1-s-Mittel)
  Val fuelL100{NAN, 0, cfg::PID_PERIOD_FAST_MS};       // Momentanverbrauch l/100 km, ab 5 km/h
  bool fuelCut = false;                                 // Schubabschaltung aktiv
  bool cutWatch = false;                                // Fuß vom Gas: Lambda bevorzugt abfragen (obdTask)
  char cutWhy[64] = "";                                 // Diagnose "Schub": Entscheidung mit Grund
  Val avg1{NAN, 0, cfg::PID_PERIOD_FAST_MS};           // Ø letzter Kilometer
  Val avg10{NAN, 0, cfg::PID_PERIOD_FAST_MS};          // Ø 10 km
  Val avg100{NAN, 0, cfg::PID_PERIOD_FAST_MS};         // Ø 100 km
  Val avgTank{NAN, 0, cfg::PID_PERIOD_FAST_MS};        // Ø seit dem Tanken
  Val avgTrip{NAN, 0, cfg::PID_PERIOD_FAST_MS};        // Ø Fahrt
  Val avgProfile{NAN, 0, cfg::PID_PERIOD_FAST_MS};     // Ø seit Profilanlage
  Val tankL{NAN, 0, cfg::PID_PERIOD_FAST_MS};          // Liter im Tank (0x2F bzw. Tankmodell)
  Val rangeKm{NAN, 0, cfg::PID_PERIOD_FAST_MS};        // Reichweite km
  Val tripKm{NAN, 0, cfg::PID_PERIOD_FAST_MS};
  Val tripL{NAN, 0, cfg::PID_PERIOD_FAST_MS};
  Val tripCost{NAN, 0, cfg::PID_PERIOD_FAST_MS};       // Euro mit Mischpreis
  Val mixPrice{NAN, 0, cfg::PID_PERIOD_FAST_MS};       // Ø-Preis im Tank €/l
  Val pumpPrice{NAN, 0, cfg::PID_PERIOD_FAST_MS};      // zuletzt eingegebener Zapfsäulenpreis
  Val tripDurationS{NAN, 0, cfg::PID_PERIOD_FAST_MS};
  Val tripIdleS{NAN, 0, cfg::PID_PERIOD_FAST_MS};
  Val fillKm{NAN, 0, cfg::PID_PERIOD_FAST_MS};         // gefahren seit dem Tanken
  Val fillL{NAN, 0, cfg::PID_PERIOD_FAST_MS};          // verbraucht seit dem Tanken
  Val sinceFullL{NAN, 0, cfg::PID_PERIOD_FAST_MS};     // berechnet seit der letzten Vollbetankung
  uint16_t refuelSeq = 0;                               // automatische Tankerkennung: zählt hoch
  uint16_t refuelAskSeq = 0;                            // ohne 0x2F: "Getankt?" fragen (zählt hoch)
  float tripSpeedRefKmh = NAN;                          // Ø Tempo der letzten Fahrten (Sport: Zeit gewonnen)
  Val tankPhysL{NAN, 0, cfg::PID_PERIOD_FAST_MS};       // physischer Tankinhalt mit Reserve
  float refuelL = NAN;                                  // erkannte Liter

  // --- Eco (calcTask, A6/A9) ---
  int8_t gear = -1;                                     // Anzeige-Nummer, 0 = "N", -1 = "–"
  bool shiftAdvice = false;                             // Hochschalten empfohlen (Pfeil nach 1 s)
  Val accel{NAN, 0, cfg::PID_PERIOD_FAST_MS};          // m/s², gefiltert
  Val pedalUsed{NAN, 0, cfg::PID_PERIOD_FAST_MS};      // Gaspedal, ersatzweise Drosselklappe %
  Val ecoScore{NAN, 0, cfg::PID_PERIOD_FAST_MS};       // laufende Fahrt
  Val cutSavedL{NAN, 0, cfg::PID_PERIOD_FAST_MS};      // Schub gespart
  Val brakedL{NAN, 0, cfg::PID_PERIOD_FAST_MS};        // Gebremst
  Val goalL100{NAN, 0, cfg::PID_PERIOD_FAST_MS};       // Spar-Ziel (Menü, Etappe 7), NAN = aus
  uint16_t gearCheckSeq = 0;                            // zählt hoch, wenn die Fahrzeug-Prüfung fragen soll
  bool hasLastTrip = false;                             // Start-Karte (Z 12)
  // --- Sport und Sprint (A10) ---
  Val powerKw{NAN, 0, cfg::PID_PERIOD_FAST_MS};        // geschätzte Leistung am Rad
  Val tripVmax{NAN, 0, cfg::PID_PERIOD_FAST_MS};
  Val tripKwPeak{NAN, 0, cfg::PID_PERIOD_FAST_MS};
  SprintInfo sprint;
  DtcInfo dtc;
  // --- Spar-Ziel, Wartung, Spartempo (Etappe 7) ---
  Val goalBase{NAN, 0, cfg::PID_PERIOD_FAST_MS};       // Spar-Ziel auto: Schnitt, aus dem es stammt
  Val odoKm{NAN, 0, cfg::PID_PERIOD_FAST_MS};          // Tachostand (eigene Zählung)
  Val oilLeftKm{NAN, 0, cfg::PID_PERIOD_FAST_MS};      // Ölwechsel fällig in … km
  Val inspLeftKm{NAN, 0, cfg::PID_PERIOD_FAST_MS};     // Inspektion fällig in … km
  struct {
    float oilIntervalKm = cfg::OIL_INTERVAL_DEFAULT_KM;
    float inspIntervalKm = cfg::INSP_INTERVAL_DEFAULT_KM;
  } maint;
  float tempoKm[cfg::TEMPO_CLASSES] = {};               // Spartempo je Klasse 30 … 130 km/h
  float tempoL[cfg::TEMPO_CLASSES] = {};
  trip::TripRecord lastTrip = {};

  // --- Optionale Sensoren (Etappe 8) ---
  bool hasImu = false;                                  // MPU6050 erkannt
  bool imuReady = false;                                // Einbaulage gelernt
  Val imuLong{NAN, 0, cfg::PID_PERIOD_FAST_MS};        // Längsbeschleunigung ohne Steigung m/s² (+ = schneller)
  Val imuLat{NAN, 0, cfg::PID_PERIOD_FAST_MS};         // Querbeschleunigung m/s² (+ = links)
  Val slopePct{NAN, 0, cfg::PID_PERIOD_FAST_MS};       // Steigung % (+ = bergauf)
  bool hasGps = false;                                  // GPS liefert NMEA
  bool gpsFix = false;
  Val gpsSpeed{NAN, 0, 2000};                           // km/h laut GPS (nur mit Fix und ≥ 5 Satelliten)
  uint8_t gpsSats = 0;
  bool gpsTimeValid = false;
  uint8_t gpsHour = 0, gpsMinute = 0;                  // Ortszeit
  uint32_t gpsDate = 0;                                 // Ortsdatum JJJJMMTT, 0 = unbekannt
  uint32_t gpsEpoch = 0;                                // UTC, s seit 1970
  float nightFactor = NAN;                              // Sonnenstand: 0 Tag … 1 Nacht, NAN ohne Fix
  bool hasSd = false;                                   // microSD erkannt (Export)
  uint16_t exportSeq = 0;                               // zählt jeden Export hoch
  char exportMsg[48] = "";                              // Ergebnis des letzten Exports
  // Thermostat-Check (A9, Z 10)
  uint16_t thermoSeq = 0;                               // zählt hoch, wenn der Hinweis einmal erscheinen soll
  bool thermoActive = false;                            // Eintrag "Hinweis · kein Fehlercode"
};

// Kopie für die UI, mit dem Zeitpunkt der Kopie
struct CarSnapshot : CarState {
  uint32_t now = 0;    // millis() beim Kopieren; Bezug für Val::fresh()
  uint32_t seq = 0;    // zählt jede Änderung am CarState hoch

  bool engineRunning() const {
    const float r = rpm.get(now);
    return !std::isnan(r) && r > cfg::ENGINE_RUNNING_MIN_RPM;
  }
};
