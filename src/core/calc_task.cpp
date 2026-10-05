#include "calc_task.h"

#include <Arduino.h>
#include <freertos/semphr.h>

#include "calc/vehicle_calc.h"
#include "config.h"
#include "core/car_state_store.h"
#include "core/commands.h"
#include "storage/storage_task.h"
#include "util/format.h"

namespace calc {

namespace {

SemaphoreHandle_t loadMutex = nullptr;
bool loadPending = false;
bool loadHasProfile = false;
bool loadHasState = false;
Profile loadProfile;
PersistState loadState;

VehicleCalc vc;
CarState input;  // Kopie des CarState für einen Rechenschritt

void takeLoad() {
  static Profile p;
  static PersistState st;
  bool pending = false, hasProfile = false, hasState = false;
  xSemaphoreTake(loadMutex, portMAX_DELAY);
  if (loadPending) {
    pending = true;
    hasProfile = loadHasProfile;
    hasState = loadHasState;
    if (hasProfile) p = loadProfile;
    if (hasState) st = loadState;
    loadPending = false;
  }
  xSemaphoreGive(loadMutex);
  if (!pending) return;

  // Das bisherige Profil zuerst sichern
  if (vc.active()) {
    vc.markSaved(millis());
    storage::requestSave(vc.state());
  }
  if (hasProfile) {
    vc.load(p, hasState ? &st : nullptr, millis());
  } else {
    vc.unload();
  }
}

void handleCommands() {
  Command c;
  while (commands::fromCalc(c, 0)) {
    if (!vc.active()) continue;
    switch (c.type) {
      case CmdType::Refuel: {
        bool cal = false;
        // i: Bit 0 = vollgetankt, Bit 1–2 = Herkunft der Liter (trip::FillSource)
        const auto src = static_cast<trip::FillSource>((c.i >> 1) & 3);
        const trip::FillRecord r = vc.refuel(c.f, c.f2, (c.i & 1) != 0, src, cal);
        Serial.printf("Getankt: %.1f l zu %.3f EUR/l%s\n", c.f, c.f2, (c.i & 1) ? ", voll" : "");
        storage::appendFill(vc.profile().id, r);
        if (cal) Serial.printf("Kalibrierung: fuel_cal jetzt %.4f\n", vc.profile().fuelCal);
        break;
      }
      case CmdType::EndTrip:
        vc.endTrip();
        break;
      case CmdType::GearCheckYes:
        vc.relearnGears();
        Serial.println("Gänge: Profil behalten, Gänge werden neu gelernt");
        break;
      case CmdType::GearCheckNo:
        vc.dismissGearCheck();
        break;
      default:
        break;
    }
  }
}

void logSave(const VehicleCalc::Outputs& o) {
  char trip[12], a10[12], tank[12], range[12];
  fmt::number(trip, sizeof(trip), o.tripKm, 1);
  fmt::number(a10, sizeof(a10), o.avg10, 1);
  fmt::number(tank, sizeof(tank), o.tankL, 1);
  fmt::number(range, sizeof(range), o.rangeKm, 0);
  const trip::TripState& t = vc.state().trip;
  Serial.printf("Eco: Score %.0f, Bewegung %.0f s, Rollen %.0f s, Pedal %.0f %%, Schaltempf. %.0f s, gebremst %.3f l, Leerlauf %.0f/%.0f s, Gang %d\n",
                o.ecoScore, t.moveS, t.rollS, t.pedalAbs, t.shiftOpenS, t.brakedL, t.idleS, t.durationS, o.gear);
  float g[MAX_GEARS];
  const int n = eco::findGears(vc.state().gearHist, g, MAX_GEARS);
  Serial.printf("Gänge: %u stabile Proben, %d Häufungen:", (unsigned)vc.state().gearHist.total, n);
  for (int i = 0; i < n; i++) Serial.printf(" %.1f", g[i]);
  Serial.println();
  Serial.printf("Gespeichert: Fahrt %s km, Ø 10 km %s l/100, Tank %s l, Reichweite %s km\n", trip, a10, tank, range);
}

// Ergebnisse in den CarState
void publish(const VehicleCalc::Outputs& o, uint32_t now, bool active) {
  carstate::modify([&](CarState& s) {
    auto put = [&](Val& v, float x) { v.set(active ? x : NAN, now); };
    put(s.fuelLph, o.instLph);
    put(s.fuelL100, o.instL100);
    s.fuelCut = active && o.fuelCut;
    put(s.avg1, o.avg1);
    put(s.avg10, o.avg10);
    put(s.avg100, o.avg100);
    put(s.avgTank, o.avgTank);
    put(s.avgTrip, o.avgTrip);
    put(s.avgProfile, o.avgProfile);
    put(s.tankL, o.tankL);
    put(s.rangeKm, o.rangeKm);
    put(s.tripKm, o.tripKm);
    put(s.tripL, o.tripL);
    put(s.tripCost, o.tripCost);
    put(s.mixPrice, o.mixPrice);
    put(s.pumpPrice, o.pumpPrice);
    put(s.tripDurationS, o.tripDurationS);
    put(s.tripIdleS, o.tripIdleS);
    put(s.fillKm, o.fillKm);
    put(s.fillL, o.fillL);
    put(s.sinceFullL, o.sinceFullL);
    if (active && o.refuelSeq != s.refuelSeq) {
      s.refuelSeq = o.refuelSeq;
      s.refuelL = o.refuelL;
    }
    s.gear = active ? o.gear : eco::GEAR_NONE;
    s.shiftAdvice = active && o.shiftAdvice;
    put(s.accel, o.accelMs2);
    put(s.pedalUsed, o.pedalPct);
    put(s.ecoScore, o.ecoScore);
    put(s.cutSavedL, o.cutSavedL);
    put(s.brakedL, o.brakedL);
    static bool askedBefore = false;
    if (active && o.gearMismatch && !askedBefore) {
      askedBefore = true;
      s.gearCheckSeq++;
    }
    put(s.powerKw, o.powerKw);
    put(s.tripVmax, o.tripVmax);
    put(s.tripKwPeak, o.tripKwPeak);
    {
      const perf::SprintMeter& m = vc.sprint();
      SprintInfo& si = s.sprint;
      si.state = active ? m.state() : perf::State::Ready;
      si.active = active && m.active();
      si.run80 = active && m.run80();
      si.elapsed = active ? m.elapsed(now) : NAN;
      si.doneAtMs = m.doneAtMs();
      si.launchSeq = m.launchSeq();
      si.last50 = m.last50();
      si.last100 = m.last100();
      si.last80120 = m.last80120();
      si.best50 = m.best().s50;
      si.best100 = m.best().s100;
      si.best80120 = m.best().s80120;
      si.lastTrace = m.lastTrace();
      si.bestTrace = m.best().trace100;
    }
    s.hasLastTrip = active && vc.state().hasLastTrip;
    if (s.hasLastTrip) s.lastTrip = vc.state().lastTrip;
    if (active) s.profile.fuelCal = vc.profile().fuelCal;
  });
}

}  // namespace

void init() { loadMutex = xSemaphoreCreateMutex(); }

void requestLoad(const Profile* p, const PersistState* saved) {
  xSemaphoreTake(loadMutex, portMAX_DELAY);
  loadPending = true;
  loadHasProfile = p != nullptr;
  loadHasState = saved != nullptr;
  if (p) loadProfile = *p;
  if (saved) loadState = *saved;
  xSemaphoreGive(loadMutex);
}

void step() {
  static uint32_t last = millis();

  takeLoad();
  handleCommands();

  {
    CarState& s = carstate::lock();
    input = s;
    carstate::unlock();
  }
  // Zeit erst nach dem Kopieren lesen: obdTask kann vorher noch Werte mit neuerem Zeitstempel
  // eintragen, die sonst als "aus der Zukunft" und damit veraltet gälten (wie snapshot())
  const uint32_t now = millis();
  const float dt = (now - last) / 1000.0f;
  last = now;
  vc.step(input, now, dt);
  publish(vc.out(), now, vc.active());

  trip::TripRecord rec;
  if (vc.takeTripRecord(rec)) storage::appendTrip(vc.profile().id, rec);
  if (vc.takeProfileChanged()) {
    const Profile& p = vc.profile();
    storage::saveProfile(p);
    Serial.printf("Profil gespeichert: fuel_cal %.4f, %u Gänge:", p.fuelCal, p.gearCount);
    for (uint8_t i = 0; i < p.gearCount; i++) Serial.printf(" %.1f", p.gears[i]);
    Serial.println();
  }
  if (vc.saveDue(now)) {
    vc.markSaved(now);
    storage::requestSave(vc.state());
    logSave(vc.out());
  }
}

void task(void*) {
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    step();
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(cfg::CALC_PERIOD_MS));
  }
}

}  // namespace calc
