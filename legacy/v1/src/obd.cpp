#include "obd.h"
#include "config.h"

#ifndef SIMULATE_OBD
#include "ble_serial.h"
#include <ELMduino.h>
#endif

namespace {

CarData car;

// Gemeinsame Ableitungen (Verbrauch, Trip) für echte und simulierte Daten.
uint32_t lastSpeedMs = 0;

void store(Metric m, float v) {
  car.value[m] = v;
  car.valid[m] = true;
  car.unsupported[m] = false;
  car.updatedMs[m] = millis();
}

void updateDerived(Metric m) {
  if (m == M_FUELRATE || m == M_MAF || m == M_SPEED) {
    float lph = NAN;
    if (car.valid[M_FUELRATE] && !car.unsupported[M_FUELRATE]) {
      lph = car.value[M_FUELRATE];
    } else if (car.valid[M_MAF]) {
      lph = car.value[M_MAF] * 3600.0f / FUEL_AFR / FUEL_DENSITY_G_PER_L;
    }
    car.litersPerHour = lph;
    float spd = car.get(M_SPEED);
    car.litersPer100km = (!isnan(lph) && spd > 3) ? lph / spd * 100.0f : NAN;
  }

  Trip& t = car.trip;
  if (m == M_SPEED) {
    uint32_t now = millis();
    if (lastSpeedMs) {
      float dtH = min<uint32_t>(now - lastSpeedMs, 2000) / 3600000.0f;
      t.distanceKm += car.value[M_SPEED] * dtH;
      if (!isnan(car.litersPerHour)) t.fuelL += car.litersPerHour * dtH;
      if (car.engineRunning()) t.driveMs += min<uint32_t>(now - lastSpeedMs, 2000);
    }
    lastSpeedMs = now;
    t.maxSpeed = max(t.maxSpeed, car.value[M_SPEED]);
  }
  if (m == M_RPM) t.maxRpm = max(t.maxRpm, car.value[M_RPM]);
  if (m == M_COOLANT && (isnan(t.maxCoolant) || car.value[m] > t.maxCoolant)) t.maxCoolant = car.value[m];
  if (m == M_VOLT && car.engineRunning() && (isnan(t.minVolt) || car.value[m] < t.minVolt)) t.minVolt = car.value[m];
}

void setStatus(const char* s) {
  strlcpy(car.status, s, sizeof(car.status));
  Serial.println(s);
}

uint32_t pollCounter = 0, pollWindowStart = 0;
void countPoll() {
  pollCounter++;
  uint32_t now = millis();
  if (now - pollWindowStart >= 1000) {
    car.pollsPerSec = pollCounter;
    pollCounter = 0;
    pollWindowStart = now;
  }
}

#ifdef SIMULATE_OBD
// ---------------------------------------------------------------------------
// Simulator: plausible Fake-Fahrt ohne Auto
// ---------------------------------------------------------------------------
uint32_t lastSim = 0;

void simulate() {
  uint32_t now = millis();
  if (now - lastSim < 100) return;
  lastSim = now;
  float t = now / 1000.0f;
  float speed = max(0.0f, 60 + 45 * sinf(t / 20) + 8 * sinf(t / 3));
  int gear = speed < 15 ? 1 : speed < 30 ? 2 : speed < 50 ? 3 : speed < 70 ? 4 : 5;
  const float ratio[] = {0, 120, 70, 48, 37, 30};
  float rpm = max(800.0f, speed * ratio[gear] / 1.0f);
  float warm = min(1.0f, t / 240.0f);  // Motor wird in 4 min warm

  store(M_SPEED, roundf(speed));         updateDerived(M_SPEED);
  store(M_RPM, rpm);                     updateDerived(M_RPM);
  store(M_LOAD, 25 + 30 * (0.5f + 0.5f * sinf(t / 3)));
  store(M_THROTTLE, 10 + 25 * (0.5f + 0.5f * sinf(t / 3)));
  store(M_COOLANT, 20 + 70 * warm + 3 * sinf(t / 30)); updateDerived(M_COOLANT);
  store(M_INTAKE, 25 + 8 * warm);
  store(M_VOLT, 14.2f + 0.15f * sinf(t / 7)); updateDerived(M_VOLT);
  store(M_MAF, rpm / 1000.0f * 4.5f * (0.4f + car.value[M_LOAD] / 100));
  updateDerived(M_MAF);
  store(M_FUELLEVEL, 62 - t / 120);
  store(M_STFT, 2.5f * sinf(t * 1.7f));
  store(M_LTFT, 3.1f);
  car.unsupported[M_FUELRATE] = true;
  car.unsupported[M_OIL] = true;
  countPoll();
}

#else
// ---------------------------------------------------------------------------
// Echtes Auto über BLE-ELM327
// ---------------------------------------------------------------------------
BleSerial SerialBT;
ELM327 elm;

using Reader = float (*)();
struct Pid {
  Metric metric;
  Reader read;
};

// Schnelle Werte: in jeder Runde
const Pid FAST[] = {
  {M_RPM,      [] { return elm.rpm(); }},
  {M_SPEED,    [] { return (float)elm.kph(); }},
  {M_LOAD,     [] { return elm.engineLoad(); }},
  {M_THROTTLE, [] { return elm.throttle(); }},
};
// Langsame Werte: pro Runde nur einer, reihum
const Pid SLOW[] = {
  {M_COOLANT,   [] { return elm.engineCoolantTemp(); }},
  {M_VOLT,      [] { return elm.batteryVoltage(); }},
  {M_FUELRATE,  [] { return elm.fuelRate(); }},
  {M_MAF,       [] { return elm.mafRate(); }},
  {M_INTAKE,    [] { return elm.intakeAirTemp(); }},
  {M_STFT,      [] { return elm.shortTermFuelTrimBank_1(); }},
  {M_LTFT,      [] { return elm.longTermFuelTrimBank_1(); }},
  {M_FUELLEVEL, [] { return elm.fuelLevel(); }},
  {M_OIL,       [] { return elm.oilTemp(); }},
};
constexpr uint8_t N_FAST = sizeof(FAST) / sizeof(FAST[0]);
constexpr uint8_t N_SLOW = sizeof(SLOW) / sizeof(SLOW[0]);

uint8_t fastIdx = 0, slowIdx = 0;
bool doSlow = false;
const Pid* current = nullptr;

uint8_t noDataCount[M_COUNT] = {0};
uint8_t consecutiveErrors = 0;
uint32_t lastAnySuccessMs = 0;
uint32_t lastConnectTry = 0;
bool announced = false;

const Pid* nextPid() {
  // Abwechselnd: alle schnellen PIDs, dann ein langsamer
  for (uint8_t guard = 0; guard < N_FAST + N_SLOW + 1; guard++) {
    const Pid* p;
    if (doSlow) {
      p = &SLOW[slowIdx];
      slowIdx = (slowIdx + 1) % N_SLOW;
      doSlow = false;
    } else {
      p = &FAST[fastIdx];
      fastIdx = (fastIdx + 1) % N_FAST;
      if (fastIdx == 0) doSlow = true;
    }
    if (!car.unsupported[p->metric]) return p;
  }
  return &SLOW[1];  // Spannung geht immer (AT-Befehl, kein PID)
}

void connect() {
  // Erst einen Durchlauf ohne Blockieren, damit das Display "Verbinde..." zeigen kann
  if (!announced) {
    setStatus("Suche OBD-Adapter ...");
    announced = true;
    return;
  }
  if (millis() - lastConnectTry < 3000 && lastConnectTry) return;
  lastConnectTry = millis();

  bool ok = SerialBT.connect(ELM_BT_NAME, ELM_BT_MAC);

  if (!ok) {
    setStatus("Adapter nicht gefunden, neuer Versuch");
    return;
  }
  car.link = LinkState::InitElm;
  setStatus("Bluetooth ok, starte ELM327 ...");
}

void initElm() {
  // Protokoll '0' = automatisch. Timeout großzügig, erste Antwort nach ATZ dauert.
  if (!elm.begin(SerialBT, false, 2000, '0')) {
    setStatus("ELM327 antwortet nicht");
    SerialBT.disconnect();
    car.link = LinkState::Connecting;
    announced = false;
    return;
  }
  car.link = LinkState::Running;
  consecutiveErrors = 0;
  current = nullptr;
  setStatus("Verbunden");
}

void poll() {
  if (!current) current = nextPid();
  float v = current->read();
  int8_t st = elm.nb_rx_state;
  if (st == ELM_GETTING_MSG) return;

  Metric m = current->metric;
  if (st == ELM_SUCCESS) {
    store(m, v);
    updateDerived(m);
    noDataCount[m] = 0;
    consecutiveErrors = 0;
    lastAnySuccessMs = millis();
    countPoll();
  } else if (st == ELM_NO_DATA) {
    // Nur als "nicht unterstützt" markieren, wenn das Auto sonst antwortet
    // (bei Zündung aus kommt für alles NO DATA).
    if (++noDataCount[m] >= 3 && millis() - lastAnySuccessMs < 5000 && !car.valid[m]) {
      car.unsupported[m] = true;
      Serial.printf("PID fuer Wert %d nicht unterstuetzt\n", m);
    }
  } else {
    consecutiveErrors++;
  }

  if (consecutiveErrors >= 10) {
    if (!SerialBT.connected()) {
      setStatus("Verbindung verloren");
      car.link = LinkState::Connecting;
      announced = false;
    } else {
      setStatus("Keine Antwort, Zuendung an?");
    }
    consecutiveErrors = 0;
  } else if (st == ELM_SUCCESS && strcmp(car.status, "Verbunden") != 0) {
    setStatus("Verbunden");
  }
  current = nullptr;
}
#endif

}  // namespace

namespace obd {

void begin() {
  for (int i = 0; i < M_COUNT; i++) {
    car.value[i] = NAN;
    car.valid[i] = false;
    car.unsupported[i] = false;
    car.updatedMs[i] = 0;
  }
#ifdef SIMULATE_OBD
  car.link = LinkState::Running;
  setStatus("Simulator");
#else
  SerialBT.begin(DEVICE_BT_NAME);
#endif
}

void update() {
#ifdef SIMULATE_OBD
  simulate();
#else
  switch (car.link) {
    case LinkState::Connecting: connect(); break;
    case LinkState::InitElm:    initElm(); break;
    case LinkState::Running:    poll();    break;
    case LinkState::Failed:     break;
  }
#endif
}

const CarData& data() { return car; }

void resetTrip() {
  car.trip = Trip{};
  lastSpeedMs = 0;
}

}  // namespace obd
