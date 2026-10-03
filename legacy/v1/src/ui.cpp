#include "ui.h"
#include <TFT_eSPI.h>
#include "config.h"
#include "history.h"

// Hinweis: Die eingebauten TFT_eSPI-Schriften kennen nur ASCII,
// deshalb "ue" statt "ü" und "C" statt "°C".

namespace {

TFT_eSPI tft;
TFT_eSprite chart(&tft);
bool chartOk = false;

constexpr int W = 320, H = 240, HEADER = 22;
constexpr uint16_t BG = TFT_BLACK, FRAME = 0x2945, LABEL = 0x9CF3, GRID = 0x2104;
constexpr uint16_t C_BLUE = 0x3D7F, C_ORANGE = 0xFC60, C_GREEN = 0x3F08, C_RED = 0xF8A3, C_YEL = 0xFF20;

enum Page : uint8_t { P_OVERVIEW, P_TEMPS, P_DRIVE, P_POWER, P_TRIP, P_COUNT };
const char* const TITLES[P_COUNT] = {"Uebersicht", "Temperaturen", "Drehzahl & Tempo", "Spannung & Verbrauch", "Fahrt & Diagnose"};

uint8_t page = P_OVERVIEW;
bool pageDirty = true;
uint32_t lastValues = 0, lastChart = 0, lastHistory = 0;

// Verläufe
History hCoolant, hIntake, hOil, hRpm, hSpeed, hVolt, hLph;

// --------------------------------------------------------------------------
// Hilfsfunktionen
// --------------------------------------------------------------------------
String fmt(float v, uint8_t decimals) {
  if (isnan(v)) return "--";
  return String(v, (unsigned int)decimals);
}

void drawHeader(const CarData& car) {
  tft.fillRect(0, 0, W, HEADER, FRAME);
  tft.setTextColor(TFT_WHITE, FRAME);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(TITLES[page], 6, HEADER / 2, 2);
  // Seitenpunkte
  for (uint8_t i = 0; i < P_COUNT; i++)
    tft.fillCircle(W - 70 + i * 9, HEADER / 2, 3, i == page ? TFT_WHITE : LABEL);
  // Verbindungsstatus
  uint16_t c = car.link == LinkState::Running ? C_GREEN : C_YEL;
  tft.fillCircle(W - 10, HEADER / 2, 5, c);
}

// --------------------------------------------------------------------------
// Seite 1: Kacheln
// --------------------------------------------------------------------------
struct Tile {
  const char* label;
  const char* unit;
};
const Tile TILES[9] = {
  {"Tempo", "km/h"},       {"Drehzahl", "1/min"},  {"Kuehlmittel", "C"},
  {"Bordspannung", "V"},   {"Ansaugluft", "C"},    {"Motorlast", "%"},
  {"Verbrauch", ""},       {"Gaspedal", "%"},      {"Tank", "%"},
};
constexpr int TW = W / 3, TH = (H - HEADER) / 3;

void drawTileFrames() {
  for (int i = 0; i < 9; i++) {
    int x = (i % 3) * TW, y = HEADER + (i / 3) * TH;
    tft.drawRect(x, y, TW, TH, FRAME);
    tft.setTextColor(LABEL, BG);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(TILES[i].label, x + 5, y + 4, 2);
  }
}

void drawTileValue(int i, const String& text, uint16_t color, const char* unit) {
  int x = (i % 3) * TW, y = HEADER + (i / 3) * TH;
  tft.setTextColor(color, BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextPadding(TW - 8);
  tft.drawString(text, x + TW / 2, y + TH / 2 + 4, 4);
  tft.setTextPadding(0);
  tft.setTextColor(LABEL, BG);
  tft.setTextDatum(BR_DATUM);
  tft.setTextPadding(40);
  tft.drawString(unit, x + TW - 5, y + TH - 3, 2);
  tft.setTextPadding(0);
}

uint16_t voltColor(const CarData& car) {
  float v = car.get(M_VOLT);
  if (isnan(v)) return TFT_WHITE;
  if (v > WARN_VOLT_HIGH) return C_RED;
  if (car.engineRunning() ? v < WARN_VOLT_CHARGE : v < WARN_VOLT_LOW) return C_RED;
  return C_GREEN;
}

uint16_t coolantColor(float c) {
  if (isnan(c)) return TFT_WHITE;
  if (c >= WARN_COOLANT_C) return C_RED;
  if (c < COLD_ENGINE_C) return C_BLUE;
  return C_GREEN;
}

String valueOrNa(const CarData& car, Metric m, uint8_t dec) {
  if (car.unsupported[m]) return "n/v";  // nicht verfügbar
  return fmt(car.get(m), dec);
}

void drawOverviewValues(const CarData& car) {
  drawTileValue(0, valueOrNa(car, M_SPEED, 0), TFT_WHITE, TILES[0].unit);
  drawTileValue(1, valueOrNa(car, M_RPM, 0), TFT_WHITE, TILES[1].unit);
  drawTileValue(2, valueOrNa(car, M_COOLANT, 0), coolantColor(car.get(M_COOLANT)), TILES[2].unit);
  drawTileValue(3, valueOrNa(car, M_VOLT, 1), voltColor(car), TILES[3].unit);
  float iat = car.get(M_INTAKE);
  drawTileValue(4, valueOrNa(car, M_INTAKE, 0), iat > WARN_INTAKE_C ? C_RED : TFT_WHITE, TILES[4].unit);
  drawTileValue(5, valueOrNa(car, M_LOAD, 0), TFT_WHITE, TILES[5].unit);
  // Verbrauch: in Fahrt l/100km, im Stand l/h
  if (!isnan(car.litersPer100km))
    drawTileValue(6, fmt(car.litersPer100km, 1), TFT_WHITE, "l/100");
  else
    drawTileValue(6, fmt(car.litersPerHour, 1), TFT_WHITE, "l/h");
  drawTileValue(7, valueOrNa(car, M_THROTTLE, 0), TFT_WHITE, TILES[7].unit);
  drawTileValue(8, valueOrNa(car, M_FUELLEVEL, 0), TFT_WHITE, TILES[8].unit);

  // Statuszeile über der unteren Kachelreihe, solange nicht verbunden
  if (car.link != LinkState::Running) {
    tft.setTextColor(C_YEL, BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextPadding(W - 4);
    tft.drawString(car.status, W / 2, H - 10, 2);
    tft.setTextPadding(0);
  }
}

// --------------------------------------------------------------------------
// Diagramme: bis zu drei Linien; die erste bestimmt die linke Achse,
// jede Linie wird auf ihren eigenen Bereich skaliert.
// --------------------------------------------------------------------------
struct Series {
  const History* h;
  uint16_t color;
  const char* name;
  float fixedLo, fixedHi;  // Mindestbereich, damit kleine Schwankungen nicht riesig wirken
  uint8_t decimals;
};

template <typename Gfx>
void drawChartOn(Gfx& g, int ox, int oy, int cw, int ch, const Series* s, uint8_t n) {
  constexpr int PADL = 34, PADR = 34, PADT = 18, PADB = 4;
  int px = ox + PADL, py = oy + PADT, pw = cw - PADL - PADR, ph = ch - PADT - PADB;

  g.fillRect(ox, oy, cw, ch, BG);
  for (int i = 0; i <= 4; i++) g.drawFastHLine(px, py + ph * i / 4, pw, GRID);
  for (int i = 0; i <= 5; i++) g.drawFastVLine(px + pw * i / 5, py, ph, GRID);

  int legendX = px;
  for (uint8_t k = 0; k < n; k++) {
    const History& h = *s[k].h;
    float lo, hi;
    bool any = h.range(lo, hi);
    if (!any) { lo = s[k].fixedLo; hi = s[k].fixedHi; }
    lo = min(lo, s[k].fixedLo);
    hi = max(hi, s[k].fixedHi);
    if (hi - lo < 1e-3f) hi = lo + 1;

    // Legende mit aktuellem Wert
    float last = h.size() ? h.at(h.size() - 1) : NAN;
    String leg = String(s[k].name) + " " + fmt(last, s[k].decimals);
    g.setTextColor(s[k].color, BG);
    g.setTextDatum(TL_DATUM);
    g.drawString(leg, legendX, oy + 2, 2);
    legendX += g.textWidth(leg, 2) + 12;

    // Achsenbeschriftung: erste Linie links, zweite rechts
    if (k < 2) {
      g.setTextDatum(k == 0 ? MR_DATUM : ML_DATUM);
      int ax = k == 0 ? px - 3 : px + pw + 3;
      g.drawString(fmt(hi, s[k].decimals), ax, py + 6, 1);
      g.drawString(fmt(lo, s[k].decimals), ax, py + ph - 4, 1);
    }

    // Linie: Puffer immer über die volle Breite, neueste Werte rechts
    int prevX = -1, prevY = 0;
    uint16_t cnt = h.size();
    for (uint16_t i = 0; i < cnt; i++) {
      float v = h.at(i);
      int x = px + pw - 1 - (int)((cnt - 1 - i) * (pw - 1) / (HISTORY_LEN - 1));
      if (isnan(v)) { prevX = -1; continue; }
      int y = py + ph - 1 - (int)((v - lo) / (hi - lo) * (ph - 1));
      if (prevX >= 0) g.drawLine(prevX, prevY, x, y, s[k].color);
      else g.drawPixel(x, y, s[k].color);
      prevX = x;
      prevY = y;
    }
  }
  // Zeitachse: 5 Minuten, Beschriftung rechts unten
  g.setTextColor(LABEL, BG);
  g.setTextDatum(BR_DATUM);
  g.drawString("-5 min ... jetzt", px + pw, py + ph - 2, 1);
}

void drawChart(const Series* s, uint8_t n) {
  int ch = H - HEADER;
  if (chartOk) {
    drawChartOn(chart, 0, 0, W, ch, s, n);
    chart.pushSprite(0, HEADER);
  } else {
    drawChartOn(tft, 0, HEADER, W, ch, s, n);  // ohne Sprite flackert es etwas
  }
}

// --------------------------------------------------------------------------
// Seite 5: Fahrt & Diagnose
// --------------------------------------------------------------------------
void row(int y, const char* label, const String& value, uint16_t color = TFT_WHITE) {
  tft.setTextColor(LABEL, BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(label, 8, y, 2);
  tft.setTextColor(color, BG);
  tft.setTextDatum(TR_DATUM);
  tft.setTextPadding(120);
  tft.drawString(value, W - 8, y, 2);
  tft.setTextPadding(0);
}

void drawTrip(const CarData& car) {
  const Trip& t = car.trip;
  int y = HEADER + 6, dy = 18;
  float avg = t.distanceKm > 0.5f && t.fuelL > 0 ? t.fuelL / t.distanceKm * 100 : NAN;
  uint32_t driveMin = t.driveMs / 60000;
  row(y, "Strecke", fmt(t.distanceKm, 1) + " km");            y += dy;
  row(y, "Fahrzeit (Motor an)", String(driveMin) + " min");        y += dy;
  row(y, "Verbraucht", fmt(t.fuelL, 2) + " l");                y += dy;
  row(y, "Durchschnitt", fmt(avg, 1) + " l/100km");            y += dy;
  row(y, "Max. Kuehlmittel", fmt(t.maxCoolant, 0) + " C", coolantColor(t.maxCoolant)); y += dy;
  row(y, "Min. Spannung (Motor an)", fmt(t.minVolt, 2) + " V",
      !isnan(t.minVolt) && t.minVolt < WARN_VOLT_CHARGE ? C_RED : TFT_WHITE); y += dy;
  row(y, "Max. Drehzahl / Tempo", fmt(t.maxRpm, 0) + " / " + fmt(t.maxSpeed, 0)); y += dy;
  row(y, "Gemischkorr. kurz / lang",
      valueOrNa(car, M_STFT, 1) + " / " + valueOrNa(car, M_LTFT, 1) + " %"); y += dy;
  row(y, "Oeltemperatur", valueOrNa(car, M_OIL, 0) + " C");    y += dy;
  row(y, "Abfragen pro Sekunde", String(car.pollsPerSec));     y += dy;
  row(y, "Status", car.status, car.link == LinkState::Running ? C_GREEN : C_YEL);
}

void recordHistory(const CarData& car) {
  hCoolant.push(car.get(M_COOLANT));
  hIntake.push(car.get(M_INTAKE));
  hOil.push(car.get(M_OIL));
  hRpm.push(car.get(M_RPM));
  hSpeed.push(car.get(M_SPEED));
  hVolt.push(car.get(M_VOLT));
  hLph.push(car.litersPerHour);
}

}  // namespace

namespace ui {

void begin() {
  tft.init();
  tft.setRotation(1);  // quer, USB-Buchse rechts
  tft.fillScreen(BG);
#ifdef TFT_BL
  ledcSetup(0, 5000, 8);
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, BACKLIGHT);
#endif
  // Mit PSRAM (Freenove S3: 8 MB) volle 16 Bit Farbtiefe, sonst 8 Bit um RAM zu sparen
  chart.setColorDepth(psramFound() ? 16 : 8);
  chartOk = chart.createSprite(W, H - HEADER) != nullptr;
  Serial.printf("Diagramm-Puffer: %s, freier Heap: %u\n", chartOk ? "ok" : "FEHLT", ESP.getFreeHeap());
}

void nextPage() {
  page = (page + 1) % P_COUNT;
  pageDirty = true;
}

void update(const CarData& car) {
  uint32_t now = millis();

  if (now - lastHistory >= HISTORY_INTERVAL_MS) {
    lastHistory = now;
    recordHistory(car);
  }

  if (pageDirty) {
    tft.fillScreen(BG);
    drawHeader(car);
    if (page == P_OVERVIEW) drawTileFrames();
    pageDirty = false;
    lastValues = lastChart = 0;
  }

  static LinkState lastLink = LinkState::Failed;
  if (car.link != lastLink) {
    lastLink = car.link;
    drawHeader(car);
    if (page == P_OVERVIEW) pageDirty = true;  // Statuszeile wegräumen
  }

  switch (page) {
    case P_OVERVIEW:
      if (now - lastValues >= 250) {
        lastValues = now;
        drawOverviewValues(car);
      }
      break;
    case P_TRIP:
      if (now - lastValues >= 1000) {
        lastValues = now;
        drawTrip(car);
      }
      break;
    default:
      if (now - lastChart >= 1000) {
        lastChart = now;
        if (page == P_TEMPS) {
          Series s[] = {{&hCoolant, C_RED, "Kuehlm.", 20, 100, 0},
                        {&hIntake, C_BLUE, "Ansaug", 0, 40, 0},
                        {&hOil, C_YEL, "Oel", 20, 100, 0}};
          drawChart(s, car.unsupported[M_OIL] ? 2 : 3);
        } else if (page == P_DRIVE) {
          Series s[] = {{&hRpm, C_ORANGE, "U/min", 0, 3000, 0},
                        {&hSpeed, C_BLUE, "km/h", 0, 100, 0}};
          drawChart(s, 2);
        } else if (page == P_POWER) {
          Series s[] = {{&hVolt, C_GREEN, "Volt", 12, 14.5, 1},
                        {&hLph, C_ORANGE, "l/h", 0, 5, 1}};
          drawChart(s, 2);
        }
      }
      break;
  }
}

}  // namespace ui
