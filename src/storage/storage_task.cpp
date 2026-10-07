#include "storage_task.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <esp_heap_caps.h>
#include <FS.h>
#include <SD_MMC.h>

#include <cmath>
#include <cstring>

#include "config.h"
#include "core/calc_task.h"
#include "core/car_state_store.h"
#include "storage/profile_match.h"
#include "storage/store.h"
#include "util/format.h"

namespace storage {

namespace {

constexpr uint32_t LOOP_MS = 50;
constexpr uint8_t QUEUE_LEN = 8;

enum class MsgType : uint8_t { SaveProfile, AppendTrip, AppendFill, Choose, Dismiss, Create, SaveUi, SaveImu, Export };
struct Msg {
  MsgType type;
  uint8_t id;
  Profile profile;
  trip::TripRecord trip;
  trip::FillRecord fill;
  UiSettings ui;
  float axes[6];
};

QueueHandle_t queue = nullptr;
SemaphoreHandle_t mutex = nullptr;  // schützt Speicher-Slot und Profilliste

// Letzter Stand von calcTask, noch nicht geschrieben. Zwei Plätze: Beim Profilwechsel kommen das
// alte und das neue Profil kurz nacheinander, keins darf das andere verdrängen.
constexpr int SLOTS = 2;
PersistState pendingState[SLOTS];
bool pendingSave[SLOTS] = {};
PersistState writing;        // Kopie, die gerade geschrieben wird (ohne Sperre)

ProfileSummary list[cfg::MAX_PROFILES];
int listCount = 0;

uint8_t activeId = 0;        // in calcTask geladenes Profil
History hist;
SemaphoreHandle_t histMutex = nullptr;
volatile bool histWanted = false;
volatile uint16_t histSeq = 0;
uint8_t beforeAskId = 0;     // war vor der Frage geladen
uint16_t handledIdent = 0;   // zuletzt ausgewertete Fahrzeugerkennung (LinkInfo::identSeq)
bool asking = false;
bool identified = false;     // seit dem Einschalten schon ein Auto einem Profil zugeordnet (nicht nur vorgeladen)
// Was das verbundene Auto über sich verrät (aus dem CarState)
struct CarIdent {
  bool known = false;
  char vin[18] = "";
  uint8_t supported[32] = {};
  int8_t protocol = -1;
};

CarIdent currentCar() {
  CarIdent c;
  CarState& s = carstate::lock();
  c.known = s.link.supportedKnown;
  snprintf(c.vin, sizeof(c.vin), "%s", s.link.vin);
  memcpy(c.supported, s.link.supported, sizeof(c.supported));
  c.protocol = s.link.protocolId;
  carstate::unlock();
  return c;
}

void send(const Msg& m) {
  if (queue && xQueueSend(queue, &m, 0) != pdTRUE) Serial.println("Speicher: Warteschlange voll, Auftrag verworfen");
}

void refreshList() {
  static ProfileSummary fresh[cfg::MAX_PROFILES];
  const int n = store::listProfiles(fresh, cfg::MAX_PROFILES);
  xSemaphoreTake(mutex, portMAX_DELAY);
  memcpy(list, fresh, sizeof(list));
  listCount = n;
  xSemaphoreGive(mutex);
  carstate::modify([&](CarState& s) { s.profile.count = static_cast<uint8_t>(n); });
}

void setAsking(bool on) {
  asking = on;
  carstate::modify([&](CarState& s) {
    if (on && !s.profile.asking) s.profile.askSeq++;
    s.profile.asking = on;
  });
}

// Profil merkt sich das Auto, mit dem es gewählt bzw. angelegt wird (A6)
void adoptCar(Profile& p) {
  const CarIdent car = currentCar();
  if (!car.known) return;
  memcpy(p.supported, car.supported, sizeof(p.supported));
  p.protocol = car.protocol;
  if (car.vin[0]) snprintf(p.vin, sizeof(p.vin), "%s", car.vin);
}

// ANNAHME: Ein Auto gehört zu genau einem Profil. Wird es einem Profil von Hand zugeordnet, vergessen
// die anderen Profile, die bisher dazu passten, VIN, PID-Liste und Protokoll. Sonst würde bei jedem
// Start wieder gefragt (gleiche PID-Liste) bzw. die Wahl über die VIN rückgängig gemacht.
void releaseOthers(uint8_t keepId) {
  const CarIdent car = currentCar();
  if (!car.known) return;
  static ProfileSummary copy[cfg::MAX_PROFILES];
  const int n = summaries(copy, cfg::MAX_PROFILES);
  static Profile p;
  for (int i = 0; i < n; i++) {
    if (copy[i].id == keepId || !profileFitsCar(copy[i], car.vin, car.supported, car.protocol)) continue;
    if (!store::loadProfile(copy[i].id, p)) continue;
    p.vin[0] = '\0';
    memset(p.supported, 0, sizeof(p.supported));
    p.protocol = -1;
    store::saveProfile(p);
    Serial.printf("Profil: %s passt nicht mehr automatisch zu diesem Auto\n", p.name);
  }
}

bool activate(uint8_t id) {
  static Profile p;
  static PersistState st;
  if (!store::loadProfile(id, p)) {
    Serial.printf("Speicher: Profil %u nicht lesbar\n", id);
    return false;
  }
  // Schon geladen: nicht neu laden, sonst gingen die Summen seit dem letzten Speichern verloren
  if (id != activeId) {
    const bool haveState = store::loadState(id, st);
    calc::requestLoad(&p, haveState ? &st : nullptr);
    Serial.printf("Profil: %s geladen (%s)\n", p.name, haveState ? "mit gespeicherten Summen" : "neu");
  }
  activeId = id;
  store::setLastProfileId(id);
  carstate::modify([&](CarState& s) {
    s.profile.id = id;
    snprintf(s.profile.name, sizeof(s.profile.name), "%s", p.name);
    s.profile.body = p.body;
    s.profile.diesel = p.fuel == FuelType::Diesel;
    s.profile.fuelCal = p.fuelCal;
    s.profile.tankL = p.tankL;
    s.profile.coldRpmLimit = p.coldRpmLimit;
    s.profile.coldCoolantC = p.coldCoolantC;
    s.profile.powerKw = p.powerKw;
    s.profile.displacementL = p.displacementL;
    s.profile.redlineRpm = p.redlineRpm;
    snprintf(s.link.vehicle, sizeof(s.link.vehicle), "%s", p.name);
  });
  return true;
}

void deactivate() {
  if (activeId) beforeAskId = activeId;
  calc::requestLoad(nullptr, nullptr);
  activeId = 0;
  carstate::modify([](CarState& s) {
    s.profile.id = 0;
    s.profile.name[0] = '\0';
    s.link.vehicle[0] = '\0';
  });
}

// Neues Profil mit den Werten des Assistenten bzw. den Standardwerten
uint8_t create(Profile p) {
  p.id = 0;
  adoptCar(p);
  if (!store::saveProfile(p)) {
    Serial.println("Speicher: Profil konnte nicht angelegt werden (voll?)");
    return 0;
  }
  refreshList();
  Serial.printf("Profil: %s angelegt (Nr. %u)\n", p.name, p.id);
  return p.id;
}

// Fertig ausgelesenes Auto einem Profil zuordnen (A6 "Profil erkennen")
void identify() {
  const CarIdent car = currentCar();
  if (!car.known) return;
  static ProfileSummary copy[cfg::MAX_PROFILES];
  const int n = summaries(copy, cfg::MAX_PROFILES);
  // Neu verbunden (Funkloch, Zündung kurz aus): Das Profil bleibt, solange das Auto keine andere VIN
  // meldet. Eine fehlende Antwort auf VIN oder PID-Liste soll nicht mitten in der Fahrt fragen.
  if (identified && activeId) {
    bool otherVin = false;
    for (int i = 0; i < n; i++)
      if (copy[i].id == activeId) otherVin = car.vin[0] && copy[i].vin[0] && strcmp(car.vin, copy[i].vin) != 0;
    if (!otherVin) return;
  }
  const ProfileMatch m = matchProfile(copy, n, car.vin, car.supported, car.protocol);
  if (m.kind == ProfileMatch::Kind::One) {
    if (m.learnVin) {
      static Profile p;
      if (store::loadProfile(m.id, p)) {
        snprintf(p.vin, sizeof(p.vin), "%s", car.vin);
        store::saveProfile(p);
        refreshList();
      }
    }
    activate(m.id);
    identified = true;
    setAsking(false);
    return;
  }
  Serial.printf("Profil: %s, frage \"Welches Fahrzeug?\"\n",
                m.kind == ProfileMatch::Kind::None ? "kein passendes" : "mehrere passende");
  deactivate();  // nichts zählen, bis das Auto feststeht
  setAsking(true);
}

// ---------- microSD: nur beim Export (A8), nie im Fahrbetrieb ----------
bool sdMounted = false;

void sdBegin() {
  SD_MMC.setPins(BOARD_PIN_SD_CLK, BOARD_PIN_SD_CMD, BOARD_PIN_SD_D0, BOARD_PIN_SD_D1, BOARD_PIN_SD_D2, BOARD_PIN_SD_D3);
  sdMounted = SD_MMC.begin("/sdcard", false, false);
  if (sdMounted) Serial.printf("Speicher: microSD erkannt (%u MB)\n", (unsigned)(SD_MMC.cardSize() / (1024 * 1024)));
  carstate::modify([](CarState& s) { s.hasSd = sdMounted; });
}

void exportMessage(const char* text) {
  Serial.printf("Export: %s\n", text);
  carstate::modify([&](CarState& s) {
    snprintf(s.exportMsg, sizeof(s.exportMsg), "%s", text);
    s.exportSeq++;
  });
}

// Zahl mit Dezimalkomma für die CSV (Excel in Deutsch), leer ohne Wert
void csvNum(char* out, size_t size, float v, int dec) {
  if (std::isnan(v)) {
    out[0] = '\0';
    return;
  }
  snprintf(out, size, "%.*f", dec, v);
  for (char* p = out; *p; p++)
    if (*p == '.') *p = ',';
}

void csvDate(char* out, size_t size, uint32_t ymd) {
  if (!ymd) {
    out[0] = '\0';
    return;
  }
  snprintf(out, size, "%02u.%02u.%04u", (unsigned)(ymd % 100), (unsigned)(ymd / 100 % 100), (unsigned)(ymd / 10000));
}

void exportCsv() {
  // Nie im Fahrbetrieb auf die SD schreiben (A8): nur bei Stillstand bzw. Motor aus
  bool moving = true;
  {
    CarState& s = carstate::lock();
    const float v = s.speed.get(millis());
    moving = !std::isnan(v) && v >= 1.0f;
    carstate::unlock();
  }
  if (moving) return exportMessage("Nur im Stand möglich");
  if (!sdMounted) sdBegin();
  if (!sdMounted) return exportMessage("Keine microSD gefunden");
  if (!activeId) return exportMessage("Kein Fahrzeugprofil geladen");
  // Die Puffer der Historie (PSRAM) mitbenutzen; danach ist die Historie ohnehin aktuell
  if (!hist.trips || !hist.fills) return exportMessage("Kein Speicher");
  xSemaphoreTake(histMutex, portMAX_DELAY);
  hist.nTrips = store::readTrips(activeId, hist.trips, cfg::TRIP_LOG_SIZE);
  hist.nFills = store::readFills(activeId, hist.fills, cfg::FILL_LOG_SIZE);
  xSemaphoreGive(histMutex);
  histSeq++;
  const trip::TripRecord* trips = hist.trips;
  const trip::FillRecord* fills = hist.fills;
  const int nt = hist.nTrips, nf = hist.nFills;
  char a[16], b[16], c[16], d[16], e[16], f[16], g[16], h[16], k[16], date[16];
  File out = SD_MMC.open("/cardisplay_fahrten.csv", FILE_WRITE);
  if (!out) return exportMessage("Datei lässt sich nicht schreiben");
  out.print("Fahrt;Datum;km;Liter;l/100 km;Dauer min;Leerlauf min;Schub min;Gebremst l;Eco-Score;max U/min;max Kühlmittel;Kosten EUR\r\n");
  for (int i = 0; i < nt; i++) {
    const trip::TripRecord& t = trips[i];
    csvDate(date, sizeof(date), t.date);
    csvNum(a, sizeof(a), t.km, 1);
    csvNum(b, sizeof(b), t.liters, 2);
    csvNum(c, sizeof(c), t.km > 0 ? t.liters / t.km * 100 : NAN, 1);
    csvNum(d, sizeof(d), t.durationS / 60, 0);
    csvNum(e, sizeof(e), t.idleS / 60, 1);
    csvNum(f, sizeof(f), t.cutS / 60, 1);
    csvNum(g, sizeof(g), t.brakedL, 2);
    csvNum(h, sizeof(h), t.ecoScore, 0);
    csvNum(k, sizeof(k), t.cost, 2);
    char maxRpm[16], maxC[16];
    csvNum(maxRpm, sizeof(maxRpm), t.maxRpm, 0);
    csvNum(maxC, sizeof(maxC), t.maxCoolantC, 0);
    out.printf("%u;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s\r\n", t.number, date, a, b, c, d, e, f, g, h, maxRpm, maxC, k);
  }
  out.close();
  out = SD_MMC.open("/cardisplay_tankfuellungen.csv", FILE_WRITE);
  if (!out) return exportMessage("Datei lässt sich nicht schreiben");
  out.print("Füllung;Datum;km seit voriger;Liter;voll;Preis EUR/l;l/100 km;EUR/100 km;Liter aus\r\n");
  static const char* const SRC[] = {"Tankanzeige", "Berechnung", "Beleg"};
  for (int i = 0; i < nf; i++) {
    const trip::FillRecord& t = fills[i];
    csvDate(date, sizeof(date), t.date);
    csvNum(a, sizeof(a), t.kmSinceLast, 0);
    csvNum(b, sizeof(b), t.liters, 2);
    csvNum(c, sizeof(c), t.price, 3);
    csvNum(d, sizeof(d), t.l100, 1);
    csvNum(e, sizeof(e), t.costPer100, 2);
    out.printf("%u;%s;%s;%s;%s;%s;%s;%s;%s\r\n", t.number, date, a, b, t.full ? "ja" : "nein", c, d, e,
               SRC[static_cast<int>(t.source) < 3 ? static_cast<int>(t.source) : 0]);
  }
  out.close();
  char msg[48];
  snprintf(msg, sizeof(msg), "%d Fahrten, %d Tankfüllungen gespeichert", nt, nf);
  exportMessage(msg);
}

void handle(const Msg& m) {
  switch (m.type) {
    case MsgType::SaveProfile: {
      // calcTask ändert nur, was es selbst lernt (fuel_cal). Alles andere kommt aus der Datei, damit
      // die Kopie in calcTask nicht überschreibt, was hier inzwischen dazukam (VIN, PID-Liste).
      static Profile p;
      if (!store::loadProfile(m.profile.id, p)) p = m.profile;
      p.fuelCal = m.profile.fuelCal;
      p.gearCount = m.profile.gearCount;  // Gänge lernt calcTask selbst (A6)
      p.body = m.profile.body;            // Fahrzeugart und Kalt-Grenze aus dem Menü
      p.coldRpmLimit = m.profile.coldRpmLimit;
      p.kmFactor = m.profile.kmFactor;  // mit GPS nachgeführt
      // Fahrzeugdaten aus dem Menü "Fahrzeug" (7.10.2026)
      p.fuel = m.profile.fuel;
      p.displacementL = m.profile.displacementL;
      p.tankL = m.profile.tankL;
      p.powerKw = m.profile.powerKw;
      p.shiftRpm = m.profile.shiftRpm;
      memcpy(p.gears, m.profile.gears, sizeof(p.gears));
      if (store::saveProfile(p)) refreshList();
      carstate::modify([&](CarState& s) {
        if (s.profile.id == p.id) s.profile.fuelCal = p.fuelCal;
      });
      break;
    }
    case MsgType::AppendTrip:
      if (store::appendTrip(m.id, m.trip)) {
        char km[12];
        fmt::number(km, sizeof(km), m.trip.km, 1);
        Serial.printf("Fahrtenbuch: Fahrt %u mit %s km gespeichert\n", m.trip.number, km);
      }
      break;
    case MsgType::AppendFill:
      store::appendFill(m.id, m.fill);
      break;
    case MsgType::Choose: {
      static Profile p;
      if (!store::loadProfile(m.id, p)) break;
      adoptCar(p);
      store::saveProfile(p);
      releaseOthers(m.id);
      refreshList();
      activate(m.id);
      identified = currentCar().known;
      setAsking(false);
      break;
    }
    case MsgType::Dismiss: {
      if (!asking) break;
      // ANNAHME: Wer die Frage ohne Wahl schließt, fährt mit dem vorher geladenen bzw. zuletzt
      // benutzten Profil weiter; gibt es noch keins, entsteht eins mit den Standardwerten (A6).
      uint8_t id = beforeAskId ? beforeAskId : store::lastProfileId();
      Profile probe;
      if (!id || !store::loadProfile(id, probe)) {
        Profile p;
        snprintf(p.name, sizeof(p.name), "%s", cfg::DEFAULT_PROFILE_NAME);
        p.applyDerivedDefaults();
        id = create(p);
      }
      if (id) activate(id);
      identified = id && currentCar().known;
      setAsking(false);
      break;
    }
    case MsgType::SaveUi:
      store::saveUi(m.ui);
      break;
    case MsgType::SaveImu:
      store::saveImuAxes(m.id, m.axes);
      break;
    case MsgType::Export:
      exportCsv();
      break;
    case MsgType::Create: {
      const uint8_t id = create(m.profile);
      if (id) {
        releaseOthers(id);
        refreshList();
        activate(id);
        identified = currentCar().known;
      }
      setAsking(false);
      break;
    }
  }
}

}  // namespace

void init() {
  queue = xQueueCreate(QUEUE_LEN, sizeof(Msg));
  mutex = xSemaphoreCreateMutex();
  histMutex = xSemaphoreCreateMutex();
  hist.trips = static_cast<trip::TripRecord*>(heap_caps_malloc(sizeof(trip::TripRecord) * cfg::TRIP_LOG_SIZE, MALLOC_CAP_SPIRAM));
  hist.fills = static_cast<trip::FillRecord*>(heap_caps_malloc(sizeof(trip::FillRecord) * cfg::FILL_LOG_SIZE, MALLOC_CAP_SPIRAM));
}

void requestHistory() { histWanted = true; }
uint16_t historySeq() { return histSeq; }

const History& lockHistory() {
  xSemaphoreTake(histMutex, portMAX_DELAY);
  return hist;
}

void unlockHistory() { xSemaphoreGive(histMutex); }

void requestSave(const PersistState& st) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  // Platz mit demselben Profil überschreiben, sonst einen freien nehmen (sonst den zweiten)
  int slot = SLOTS - 1;
  for (int i = SLOTS - 1; i >= 0; i--)
    if (!pendingSave[i] || pendingState[i].profileId == st.profileId) slot = i;
  for (int i = 0; i < SLOTS; i++)
    if (pendingSave[i] && pendingState[i].profileId == st.profileId) slot = i;
  pendingState[slot] = st;
  pendingSave[slot] = true;
  xSemaphoreGive(mutex);
}

void saveProfile(const Profile& p) {
  static Msg m;  // nur von calcTask aufgerufen
  m.type = MsgType::SaveProfile;
  m.profile = p;
  send(m);
}

void appendTrip(uint8_t profileId, const trip::TripRecord& r) {
  static Msg m;  // nur von calcTask aufgerufen
  m.type = MsgType::AppendTrip;
  m.id = profileId;
  m.trip = r;
  send(m);
}

void appendFill(uint8_t profileId, const trip::FillRecord& r) {
  static Msg m;  // nur von calcTask aufgerufen
  m.type = MsgType::AppendFill;
  m.id = profileId;
  m.fill = r;
  send(m);
}

void chooseProfile(uint8_t id) {
  static Msg m;  // nur von uiTask aufgerufen
  m.type = MsgType::Choose;
  m.id = id;
  send(m);
}

void dismissChoice() {
  static Msg m;  // nur von uiTask aufgerufen
  m.type = MsgType::Dismiss;
  send(m);
}

void createProfile(const Profile& p) {
  static Msg m;  // nur von uiTask aufgerufen
  m.type = MsgType::Create;
  m.profile = p;
  send(m);
}

void saveImuAxes(uint8_t profileId, const float axes[6]) {
  static Msg m;  // nur von sensorTask aufgerufen
  m.type = MsgType::SaveImu;
  m.id = profileId;
  memcpy(m.axes, axes, sizeof(m.axes));
  send(m);
}

void requestExport() {
  static Msg m;  // nur von uiTask aufgerufen
  m.type = MsgType::Export;
  send(m);
}

void saveUi(const UiSettings& ui) {
  static Msg m;  // nur von uiTask aufgerufen
  m.type = MsgType::SaveUi;
  m.ui = ui;
  send(m);
}

int summaries(ProfileSummary* out, int max) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  const int n = listCount < max ? listCount : max;
  memcpy(out, list, sizeof(ProfileSummary) * n);
  xSemaphoreGive(mutex);
  return n;
}

void begin() {
  store::begin();
#ifndef SIMULATE_OBD
  sdBegin();  // nur erkennen; geschrieben wird erst beim Export
#endif
  refreshList();
  // Zuletzt benutztes Profil gleich laden: Mittelwerte, Tank und Reichweite stehen schon vor dem
  // Verbinden da. Passt das Auto nicht dazu, wechselt die Erkennung bzw. es kommt die Frage.
  const uint8_t last = store::lastProfileId();
  if (last) activate(last);
}

void poll() {
  Msg m;
  while (xQueueReceive(queue, &m, 0) == pdTRUE) handle(m);

  for (int i = 0; i < SLOTS; i++) {
    bool doSave = false;
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (pendingSave[i]) {
      writing = pendingState[i];
      pendingSave[i] = false;
      doSave = true;
    }
    xSemaphoreGive(mutex);
    if (doSave && !store::saveState(writing)) Serial.println("Speicher: Summen konnten nicht gespeichert werden");
  }

  // Historie für die UI lesen (nach einem Wunsch der Historie-Seite)
  if (histWanted && hist.trips && hist.fills) {
    histWanted = false;
    xSemaphoreTake(histMutex, portMAX_DELAY);
    hist.nTrips = activeId ? store::readTrips(activeId, hist.trips, cfg::TRIP_LOG_SIZE) : 0;
    hist.nFills = activeId ? store::readFills(activeId, hist.fills, cfg::FILL_LOG_SIZE) : 0;
    xSemaphoreGive(histMutex);
    histSeq++;
  }

  uint16_t ident = 0;
  {
    CarState& s = carstate::lock();
    ident = s.link.identSeq;
    carstate::unlock();
  }
  if (ident != handledIdent) {
    handledIdent = ident;
    identify();
  }
}

void task(void*) {
  begin();
  for (;;) {
    poll();
    vTaskDelay(pdMS_TO_TICKS(LOOP_MS));
  }
}

}  // namespace storage
