// Host-Harness: führt ui::task mit simulierter Zeit aus, füttert den CarState aus DriveSim,
// spielt Touch-Gesten und BOOT-Taste ab und speichert Bildschirmfotos (PPM). Nicht Teil der Firmware.
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <freertos/queue.h>

#include <cstdlib>
#include <deque>
#include <string>
#include <vector>

#include "config.h"
#include "core/calc_task.h"
#include "core/car_state_store.h"
#include "core/commands.h"
#include "sim/sim_link.h"
#include "sim/simulator.h"
#include "storage/storage_task.h"
#include "ui/ui.h"

HostSerial Serial;
TwoWire Wire;
static uint32_t g_ms = 0;
static uint16_t fb[240][320];
static int wx, wy, ww, wh, wpos;

uint32_t millis() { return g_ms; }
void pinMode(int, int) {}
void digitalWrite(int, int) {}
static bool bootDown = false;
int digitalRead(int pin) { return (pin == BOARD_PIN_BOOT && bootDown) ? LOW : HIGH; }
static int backlight = -1;
double ledcSetup(uint8_t, uint32_t f, uint8_t) { return f; }
void ledcAttachPin(uint8_t, uint8_t) {}
void ledcWrite(uint8_t, uint32_t d) { backlight = (int)d; }
TickType_t xTaskGetTickCount() { return g_ms; }
void vTaskDelete(void*) {}

struct Q { size_t item; std::deque<std::vector<uint8_t>> d; };
QueueHandle_t xQueueCreate(size_t, size_t item) { return new Q{item, {}}; }
BaseType_t xQueueSend(QueueHandle_t q, const void* it, TickType_t) {
  auto* qq = (Q*)q; qq->d.emplace_back((const uint8_t*)it, (const uint8_t*)it + qq->item); return pdTRUE; }
BaseType_t xQueueReceive(QueueHandle_t q, void* it, TickType_t) {
  auto* qq = (Q*)q; if (qq->d.empty()) return pdFALSE; memcpy(it, qq->d.front().data(), qq->item); qq->d.pop_front(); return pdTRUE; }

// Touch: geplanter Finger in Display-Koordinaten
static bool fingerDown = false; static int fx = 0, fy = 0; static uint8_t reg = 0; static int rdIdx = 0; static uint8_t rd[5];
bool TwoWire::begin(int, int, uint32_t) { return true; }
void TwoWire::beginTransmission(uint8_t) {}
size_t TwoWire::write(uint8_t b) { reg = b; return 1; }
uint8_t TwoWire::endTransmission(bool) { return 0; }
uint8_t TwoWire::requestFrom(uint8_t, uint8_t len) {
  // Umkehrung der Abbildung im Board-Header: x = roh_y, y = 239 - roh_x
  int rx = 239 - fy, ry = fx;
  rd[0] = fingerDown ? 1 : 0; rd[1] = (rx >> 8) & 0x0F; rd[2] = rx & 0xFF; rd[3] = (ry >> 8) & 0x0F; rd[4] = ry & 0xFF;
  rdIdx = 0; (void)reg; return len; }
int TwoWire::read() { return rd[rdIdx++]; }

void TFT_eSPI::init() {}
void TFT_eSPI::setRotation(uint8_t) {}
void TFT_eSPI::setSwapBytes(bool) {}
void TFT_eSPI::fillScreen(uint32_t) {}
bool TFT_eSPI::initDMA(bool) { return true; }
void TFT_eSPI::startWrite() {}
void TFT_eSPI::endWrite() {}
void TFT_eSPI::dmaWait() {}
void TFT_eSPI::setAddrWindow(int32_t x, int32_t y, int32_t w, int32_t h) { wx = x; wy = y; ww = w; wh = h; wpos = 0; }
void TFT_eSPI::pushPixels(const void* data, uint32_t len) {
  const uint16_t* p = (const uint16_t*)data;
  for (uint32_t i = 0; i < len; i++, wpos++) { int x = wx + wpos % ww, y = wy + wpos / ww; if (x < 320 && y < 240) fb[y][x] = p[i]; } }
void TFT_eSPI::pushPixelsDMA(uint16_t* d, uint32_t len) { pushPixels(d, len); }

static void shot(const std::string& name) {
  std::string path = std::string(getenv("SHOTDIR") ? getenv("SHOTDIR") : ".") + "/" + name + ".ppm";
  FILE* f = fopen(path.c_str(), "wb"); fprintf(f, "P6 320 240 255\n");
  for (int y = 0; y < 240; y++) for (int x = 0; x < 320; x++) { uint16_t c = fb[y][x];
    uint8_t rgb[3] = {(uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31)}; fwrite(rgb, 1, 3, f); }
  fclose(f); printf("[shot] %s bei %.1f s\n", name.c_str(), g_ms / 1000.0); }

// Ablauf: Zeit (ms) und Aktion
struct Step { uint32_t at; std::string act; int a, b, c, d; };
static std::vector<Step> script;
static size_t si = 0;
// laufende Wischbewegung
static int gx0, gy0, gx1, gy1; static uint32_t gStart = 0, gDur = 0; static bool gActive = false;

static DriveSim drive;
static uint32_t simNext = 0, lastMed = 0, lastSlow = 0, lastRare = 0;
static bool feedSim = true;
static int ovState = -1, ovError = 0, ovRetry = 0;  // Verbindungszustand von Hand (Aktion "link")

static void harnessStep() {
  // Simulator wie sim_task
  if (ovState >= 0) {
    carstate::modify([&](CarState& s) { s.link.state = (LinkState)ovState; s.link.error = (LinkError)ovError; s.link.retryInS = ovRetry; });
    simNext = g_ms + cfg::SIM_STEP_MS;
  } else if (g_ms < simlink::T_RUNNING) {
    carstate::modify([&](CarState& s) { s.simulated = true; simlink::update(s.link, g_ms); });
    simNext = g_ms + cfg::SIM_STEP_MS;
  }
  while (feedSim && ovState < 0 && g_ms >= simlink::T_RUNNING && g_ms >= simNext) {
    simNext += cfg::SIM_STEP_MS;
    Command c; while (commands::fromObd(c, 0)) if (c.type == CmdType::SimSprint) drive.requestSprint();
    drive.step(cfg::SIM_STEP_MS / 1000.0f);
    const SimOutput& o = drive.out(); const uint32_t now = g_ms;
    auto due = [&](uint32_t& last, uint32_t p) { if (last != 0 && now - last < p) return false; last = now; return true; };
    bool med = due(lastMed, cfg::PID_PERIOD_MEDIUM_MS), slow = due(lastSlow, cfg::PID_PERIOD_SLOW_MS), rare = due(lastRare, cfg::PID_PERIOD_RARE_MS);
    carstate::modify([&](CarState& s) {
      simlink::update(s.link, g_ms);
      s.speed.set(o.speedKmh, now); s.rpm.set(o.rpm, now); s.map.set(o.mapKpa, now); s.throttle.set(o.throttlePct, now); s.pedal.set(o.pedalPct, now);
      if (med) { s.iat.set(o.iatC, now); s.stft.set(o.stftPct, now); s.ltft.set(o.ltftPct, now); s.fuelSys.set(o.fuelSys, now); s.load.set(o.loadPct, now); }
      if (slow) { s.coolant.set(o.coolantC, now); s.voltage.set(o.voltage, now); s.fuelLevel.set(o.fuelLevelPct, now); }
      if (rare) { s.mil.set(o.mil, now); s.dtcCount.set(o.dtcCount, now); }
    });
  }
  // calcTask und storageTask im selben Takt wie auf dem Gerät
  if (g_ms % cfg::CALC_PERIOD_MS == 0) calc::step();
  if (g_ms % 50 == 0) storage::poll();
  if (gActive) {
    uint32_t t = g_ms - gStart;
    if (t >= gDur) { fingerDown = false; gActive = false; }
    else { fingerDown = true; fx = gx0 + (gx1 - gx0) * (int)t / (int)gDur; fy = gy0 + (gy1 - gy0) * (int)t / (int)gDur; }
  }
  while (si < script.size() && g_ms >= script[si].at) {
    const Step& s = script[si++];
    if (s.act == "shot") shot(std::string("shot") + std::to_string(s.a));
    else if (s.act == "drag") { gx0 = s.a; gy0 = s.b; gx1 = s.c; gy1 = s.d; gStart = g_ms; gDur = 300; gActive = true; }
    else if (s.act == "hold") { gx0 = gx1 = s.a; gy0 = gy1 = s.b; gStart = g_ms; gDur = s.c; gActive = true; }
    else if (s.act == "boot") bootDown = true;
    else if (s.act == "bootup") bootDown = false;
    else if (s.act == "nosim") feedSim = false;
    else if (s.act == "link") { ovState = s.a; ovError = s.b; ovRetry = s.c; }
    else if (s.act == "vin") carstate::modify([&](CarState& st) { snprintf(st.link.vin, sizeof(st.link.vin), "%s", s.a ? "VF1JP0A0H12345678" : ""); });
    else if (s.act == "end") { printf("Ende, Hintergrundlicht-Duty %d\n", backlight); exit(0); }
  }
}

void vTaskDelay(TickType_t t) { for (TickType_t i = 0; i < t; i++) { g_ms++; harnessStep(); } }
void vTaskDelayUntil(TickType_t* prev, TickType_t inc) { *prev += inc; while (g_ms < *prev) { g_ms++; harnessStep(); } }

int main(int argc, char** argv) {
  // Ablauf aus Datei: je Zeile "ms aktion a b c d"
  FILE* f = fopen(argc > 1 ? argv[1] : "script.txt", "r");
  char act[32]; unsigned at; int a, b, c, d;
  while (f && fscanf(f, "%u %31s %d %d %d %d", &at, act, &a, &b, &c, &d) == 6) script.push_back({at, act, a, b, c, d});
  if (f) fclose(f);
  carstate::init(); commands::init(); calc::init(); storage::init(); storage::begin();
  ui::task(nullptr);
}
