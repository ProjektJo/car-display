// Seite "Historie" (U Seite 9): vier Ansichten, oben per Chip umschaltbar.
// - Fahrten: Balken Ø l/100 km der letzten 20 Fahrten, gestrichelt der Gesamtschnitt; Tippen zeigt Details
// - Tankfüllungen: Ø l/100 km je Füllung und gestrichelt € je 100 km, darunter letzte Füllung und Trend
// - Auswertung: Ø nach Streckenlänge, Summen der letzten 50 Fahrten, Eco-Score-Trend, beste Fahrt, Schubanteil
// - Spartempo: l/100 km je Tempo (nur ebene Konstantfahrt im höchsten Gang), sparsamster Punkt
// Positionen aus der Vorschau (pHistorie). Die Daten liest storageTask beim Öffnen der Seite.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "page.h"
#include "storage/storage_task.h"
#include "ui/symbols.h"
#include "ui/theme.h"
#include "util/format.h"

namespace {

constexpr int TABS = 4;
const char* const TAB_NAMES[TABS] = {"Fahrten", "Tankfüll.", "Auswertung", "Spartempo"};
constexpr int32_t TOP = 24;          // Inhalt unter den Chips
constexpr int MAX_BARS = 20;
constexpr int MAX_FILLS = 20;

// Fahrten (Vorschau: x0 26, y0 12, h 88, w 284, Achse 0–10)
constexpr int32_t F_X0 = 26, F_Y0 = TOP + 12, F_H = 88, F_W = 284;
constexpr float F_TOP = 10.0f;
// Spartempo (x0 26, x1 300, y0 22, h 98, Achse 3–8)
constexpr int32_t S_X0 = 26, S_X1 = 300, S_Y0 = TOP + 22, S_H = 98;
constexpr float S_LO = 3.0f, S_HI = 8.0f;
// Tankfüllungen (x0 26, x1 290, y0 30, h 100)
constexpr int32_t T_X0 = 26, T_X1 = 290, T_Y0 = TOP + 30, T_H = 100;
// Auswertung: Balken je Streckenlänge
constexpr int32_t A_X0 = 12, A_Y0 = TOP + 20, A_H = 80, A_BAR_W = 28, A_STEP = 38;

void line(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color, int32_t w, int32_t dash = 0) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.p1.x = x1;
  d.p1.y = y1;
  d.p2.x = x2;
  d.p2.y = y2;
  d.color = theme::c(color);
  d.width = w;
  d.round_start = d.round_end = w > 1;
  d.dash_width = dash;
  d.dash_gap = dash;
  lv_draw_line(layer, &d);
}

void text(lv_layer_t* layer, const char* t, int32_t x, int32_t y, int32_t w, lv_text_align_t al, uint32_t color,
          const lv_font_t* font = &font_small) {
  lv_draw_label_dsc_t d;
  lv_draw_label_dsc_init(&d);
  d.text = t;
  d.text_local = 1;
  d.font = font;
  d.color = theme::c(color);
  d.align = al;
  lv_area_t a = {x, y, x + w - 1, y + lv_font_get_line_height(font)};
  lv_draw_label(layer, &d, &a);
}

void rect(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color, int32_t radius, uint32_t border = 0) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = radius;
  d.bg_color = theme::c(color);
  d.bg_opa = LV_OPA_COVER;
  if (border) {
    d.border_color = theme::c(border);
    d.border_width = 1;
    d.border_opa = LV_OPA_COVER;
  }
  lv_area_t a = {x1, y1, x2, y2};
  lv_draw_rect(layer, &d, &a);
}

void dot(lv_layer_t* layer, float x, float y, int32_t r, uint32_t color) {
  rect(layer, static_cast<int32_t>(std::lround(x)) - r, static_cast<int32_t>(std::lround(y)) - r,
       static_cast<int32_t>(std::lround(x)) + r, static_cast<int32_t>(std::lround(y)) + r, color, LV_RADIUS_CIRCLE);
}

// Runde Kurve durch die Punkte (Catmull-Rom als Bezier, wie auf der Eco-Seite)
void smooth(lv_layer_t* layer, const float* px, const float* py, int n, uint32_t color, int32_t w, bool dashed) {
  if (n < 2) return;
  float prevX = px[0], prevY = py[0];
  int seg = 0;
  for (int i = 0; i < n - 1; i++) {
    const float x0 = px[i > 0 ? i - 1 : i], y0 = py[i > 0 ? i - 1 : i];
    const float x1 = px[i], y1 = py[i], x2 = px[i + 1], y2 = py[i + 1];
    const float x3 = px[i + 2 < n ? i + 2 : i + 1], y3 = py[i + 2 < n ? i + 2 : i + 1];
    const float c1x = x1 + (x2 - x0) / 6, c1y = y1 + (y2 - y0) / 6, c2x = x2 - (x3 - x1) / 6, c2y = y2 - (y3 - y1) / 6;
    for (int k = 1; k <= 8; k++, seg++) {
      const float u = k / 8.0f, m = 1 - u;
      const float x = m * m * m * x1 + 3 * m * m * u * c1x + 3 * m * u * u * c2x + u * u * u * x2;
      const float y = m * m * m * y1 + 3 * m * m * u * c1y + 3 * m * u * u * c2y + u * u * u * y2;
      if (!dashed || seg % 2 == 0)
        line(layer, static_cast<int32_t>(std::lround(prevX)), static_cast<int32_t>(std::lround(prevY)),
             static_cast<int32_t>(std::lround(x)), static_cast<int32_t>(std::lround(y)), color, w);
      prevX = x;
      prevY = y;
    }
  }
}

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

uint32_t scoreColor(float s) {
  if (std::isnan(s)) return theme::TEXT;
  return s >= cfg::SCORE_GOOD ? theme::GOOD : (s < cfg::SCORE_OK ? theme::WARN : theme::TEXT);
}

// Zusammengefasste Daten (beim Laden berechnet)
struct TripBar {
  uint16_t number;
  uint32_t date;  // JJJJMMTT mit GPS, sonst 0
  float km, avg, min, cost, braked, score;
};

class HistoryPage : public Page {
 public:
  HistoryPage() : Page("Historie") {}

  void create(lv_obj_t* parent) override {
    lv_obj_t* tabs = lv_obj_create(parent);
    lv_obj_remove_style_all(tabs);
    lv_obj_set_size(tabs, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(tabs, LV_ALIGN_TOP_MID, 0, 3);
    lv_obj_set_flex_flow(tabs, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(tabs, 4, 0);
    lv_obj_remove_flag(tabs, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < TABS; i++) {
      lv_obj_t* c = lv_obj_create(tabs);
      lv_obj_remove_style_all(c);
      lv_obj_set_size(c, LV_SIZE_CONTENT, 18);
      lv_obj_set_style_radius(c, 9, 0);
      lv_obj_set_style_border_width(c, 1, 0);
      lv_obj_set_style_pad_hor(c, 7, 0);
      lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
      lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_ext_click_area(c, 4);
      lv_obj_add_event_cb(c, onTab, LV_EVENT_SHORT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
      lv_obj_t* l = theme::label(c, &font_m12, false, TAB_NAMES[i]);
      lv_obj_center(l);
      chips_[i] = c;
    }
    canvas_ = lv_obj_create(parent);
    lv_obj_remove_style_all(canvas_);
    lv_obj_set_pos(canvas_, 0, 0);
    lv_obj_set_size(canvas_, BOARD_LCD_HOR_RES, theme::CONTENT_H);
    lv_obj_add_event_cb(canvas_, onDraw, LV_EVENT_DRAW_MAIN, this);
    lv_obj_add_flag(canvas_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(canvas_, onCanvasTap, LV_EVENT_SHORT_CLICKED, this);
    lv_obj_add_flag(canvas_, LV_OBJ_FLAG_EVENT_BUBBLE);  // langes Drücken öffnet weiterhin das Menü
    lv_obj_move_background(canvas_);
    showTab(0);
  }

  void onShow() override {
    storage::requestHistory();
    tempoShown_ = false;
  }

  void update(const CarSnapshot& s) override {
    snap_ = s;
    if (storage::historySeq() != loadedSeq_) {
      loadedSeq_ = storage::historySeq();
      load();
      lv_obj_invalidate(canvas_);
    }
    // Spartempo kommt aus dem CarState; beim Öffnen einmal übernehmen
    if (!tempoShown_) {
      tempoShown_ = true;
      if (tab_ == 3) lv_obj_invalidate(canvas_);
    }
  }

 private:
  // ---------- Daten ----------
  void load() {
    const storage::History& h = storage::lockHistory();
    // Fahrten: letzte 20 als Balken, Summen über alle (bis 50)
    nBars_ = 0;
    totKm_ = totL_ = totCost_ = 0;
    totCutS_ = totDurS_ = 0;
    float cut10 = 0, dur10 = 0;
    bestIdx_ = -1;
    for (int b = 0; b < 4; b++) bucketKm_[b] = bucketL_[b] = 0;
    float s10 = 0, sPrev = 0;
    int n10 = 0, nPrev = 0;
    for (int i = 0; i < h.nTrips; i++) {
      const trip::TripRecord& t = h.trips[i];
      totKm_ += t.km;
      totL_ += t.liters;
      if (!std::isnan(t.cost)) totCost_ += t.cost;
      totCutS_ += t.cutS;
      totDurS_ += t.durationS;
      const int b = t.km < 5 ? 0 : (t.km < 20 ? 1 : (t.km < 50 ? 2 : 3));
      bucketKm_[b] += t.km;
      bucketL_[b] += t.liters;
      const int fromEnd = h.nTrips - 1 - i;
      if (fromEnd < 10) {
        if (!std::isnan(t.ecoScore)) { s10 += t.ecoScore; n10++; }
        cut10 += t.cutS;
        dur10 += t.durationS;
      } else if (fromEnd < 20 && !std::isnan(t.ecoScore)) {
        sPrev += t.ecoScore;
        nPrev++;
      }
      if (t.km > 5 && t.km > 0) {
        const float avg = t.liters / t.km * 100;
        if (bestIdx_ < 0 || avg < bestAvg_) {
          bestIdx_ = i;
          bestAvg_ = avg;
          bestNumber_ = t.number;
        }
      }
    }
    trend10_ = n10 ? s10 / n10 : NAN;
    trendPrev_ = nPrev ? sPrev / nPrev : NAN;
    cutShare10_ = dur10 > 0 ? cut10 / dur10 : NAN;
    nTrips_ = h.nTrips;
    const int first = h.nTrips > MAX_BARS ? h.nTrips - MAX_BARS : 0;
    for (int i = first; i < h.nTrips; i++) {
      const trip::TripRecord& t = h.trips[i];
      TripBar& b = bars_[nBars_++];
      b.number = t.number;
      b.date = t.date;
      b.km = t.km;
      b.avg = t.km > 0 ? t.liters / t.km * 100 : NAN;
      b.min = t.durationS / 60.0f;
      b.cost = t.cost;
      b.braked = t.brakedL;
      b.score = t.ecoScore;
    }
    // Tankfüllungen: letzte 20
    nFills_ = 0;
    const int f0 = h.nFills > MAX_FILLS ? h.nFills - MAX_FILLS : 0;
    for (int i = f0; i < h.nFills; i++) {
      const trip::FillRecord& f = h.fills[i];
      fillAvg_[nFills_] = f.l100;
      fillCost100_[nFills_] = f.costPer100;
      fillL_[nFills_] = f.liters;
      fillPrice_[nFills_] = f.price;
      nFills_++;
    }
    storage::unlockHistory();
    if (sel_ >= nBars_) sel_ = -1;
  }

  // ---------- Zeichnen ----------
  static void onDraw(lv_event_t* e) {
    auto* p = static_cast<HistoryPage*>(lv_event_get_user_data(e));
    lv_area_t a;
    lv_obj_get_coords(lv_event_get_target_obj(e), &a);
    lv_layer_t* layer = lv_event_get_layer(e);
    switch (p->tab_) {
      case 0: p->drawTrips(layer, a.x1, a.y1); break;
      case 1: p->drawFills(layer, a.x1, a.y1); break;
      case 2: p->drawStats(layer, a.x1, a.y1); break;
      default: p->drawTempo(layer, a.x1, a.y1); break;
    }
  }

  void drawTrips(lv_layer_t* layer, int32_t ox, int32_t oy) {
    char t[48];
    auto sy = [&](float v) { return oy + F_Y0 + F_H - static_cast<int32_t>(clampf(v, 0, F_TOP) / F_TOP * F_H); };
    for (int v = 0; v <= 10; v += 5) {
      line(layer, ox + F_X0, sy(v), ox + F_X0 + F_W, sy(v), theme::LINE, 1);
      snprintf(t, sizeof(t), "%d", v);
      text(layer, t, ox, sy(v) - 6, F_X0 - 5, LV_TEXT_ALIGN_RIGHT, theme::MUTED);
    }
    if (nBars_ == 0) {
      text(layer, "Noch keine Fahrten gespeichert", ox, oy + F_Y0 + F_H / 2 - 8, BOARD_LCD_HOR_RES, LV_TEXT_ALIGN_CENTER, theme::MUTED,
           &font_m12);
      return;
    }
    const float all = totKm_ > 0 ? totL_ / totKm_ * 100 : NAN;
    const float bw = F_W / static_cast<float>(MAX_BARS);
    for (int i = 0; i < nBars_; i++) {
      const TripBar& b = bars_[i];
      if (std::isnan(b.avg)) continue;
      const uint32_t col = std::isnan(all) ? theme::MUTED
                           : b.avg < all * cfg::COLOR_GOOD_BELOW ? theme::GOOD
                           : b.avg > all * cfg::COLOR_WARN_ABOVE ? theme::WARN
                                                                 : theme::MUTED;
      const int32_t x = ox + F_X0 + static_cast<int32_t>(i * bw) + 2;
      rect(layer, x, sy(b.avg), x + static_cast<int32_t>(bw) - 4, oy + F_Y0 + F_H, col, 2,
           (sel_ == i || (sel_ < 0 && i == nBars_ - 1)) ? theme::TEXT : 0);
    }
    if (!std::isnan(all)) {
      line(layer, ox + F_X0, sy(all), ox + F_X0 + F_W, sy(all), theme::ACCENT, 1, 3);
      char n[12];
      fmt::number(n, sizeof(n), all, 1);
      snprintf(t, sizeof(t), SYM_AVG " %s", n);
      // Beschriftung über der Linie am linken Rand, mit Hintergrund, damit Balken sie nicht verdecken
      rect(layer, ox + F_X0 + 2, sy(all) - 15, ox + F_X0 + 44, sy(all) - 2, theme::BG, 3);
      text(layer, t, ox + F_X0 + 4, sy(all) - 15, 40, LV_TEXT_ALIGN_LEFT, theme::ACCENT);
    }
    // Details der gewählten bzw. letzten Fahrt
    const int i = sel_ >= 0 ? sel_ : nBars_ - 1;
    const TripBar& b = bars_[i];
    const int32_t by = oy + TOP + 110;
    rect(layer, ox + 10, by, ox + BOARD_LCD_HOR_RES - 10, by + 52, theme::SURFACE, theme::RADIUS_TILE);
    if (b.date)  // mit GPS zusätzlich das Datum (U Seite 9)
      snprintf(t, sizeof(t), "Fahrt %u " SYM_DOT " %02u.%02u.%s", b.number, (unsigned)(b.date % 100),
               (unsigned)(b.date / 100 % 100), sel_ < 0 ? " (letzte)" : "");
    else
      snprintf(t, sizeof(t), "Fahrt %u%s", b.number, sel_ < 0 ? " (letzte)" : "");
    text(layer, t, ox + 19, by + 4, 150, LV_TEXT_ALIGN_LEFT, theme::TEXT, &font_m12);
    char n[16];
    fmt::number(n, sizeof(n), b.score, 0);
    text(layer, n, ox + BOARD_LCD_HOR_RES - 49, by + 4, 30, LV_TEXT_ALIGN_RIGHT, scoreColor(b.score), &font_m12);
    text(layer, "Eco-Score", ox + BOARD_LCD_HOR_RES - 130, by + 6, 78, LV_TEXT_ALIGN_RIGHT, theme::MUTED);
    const char* labels[5] = {"Strecke", SYM_AVG, "Dauer", "Kosten", "Gebremst"};
    char vals[5][20];
    fmt::number(n, sizeof(n), b.km, 1);
    snprintf(vals[0], sizeof(vals[0]), "%s km", n);
    fmt::number(n, sizeof(n), b.avg, 1);
    snprintf(vals[1], sizeof(vals[1]), "%s l/100", n);
    snprintf(vals[2], sizeof(vals[2]), "%d min", static_cast<int>(std::lround(b.min)));
    fmt::number(n, sizeof(n), b.cost, 2);
    snprintf(vals[3], sizeof(vals[3]), std::isnan(b.cost) ? "%s" : "%s " SYM_EURO, n);
    fmt::number(n, sizeof(n), b.braked, 2);
    snprintf(vals[4], sizeof(vals[4]), std::isnan(b.braked) ? "%s" : "%s l", n);
    static const int32_t COLS[5] = {19, 76, 142, 192, 248};
    for (int c = 0; c < 5; c++) {
      text(layer, labels[c], ox + COLS[c], by + 22, 60, LV_TEXT_ALIGN_LEFT, theme::MUTED);
      text(layer, vals[c], ox + COLS[c], by + 34, 66, LV_TEXT_ALIGN_LEFT, theme::TEXT, &font_m12);
    }
  }

  void drawFills(lv_layer_t* layer, int32_t ox, int32_t oy) {
    text(layer, "\xE2\x80\x94 l/100 km", ox + 10, oy + TOP + 4, 80, LV_TEXT_ALIGN_LEFT, theme::ACCENT);
    text(layer, "- - " SYM_EURO " pro 100 km", ox + 80, oy + TOP + 4, 100, LV_TEXT_ALIGN_LEFT, theme::MUTED);
    if (nFills_ < 1) {
      text(layer, "Noch keine Tankfüllungen gespeichert", ox, oy + T_Y0 + T_H / 2 - 8, BOARD_LCD_HOR_RES, LV_TEXT_ALIGN_CENTER,
           theme::MUTED, &font_m12);
      return;
    }
    // Achsen aus den Daten, rund
    float lo = 100, hi = 0, lo2 = 1000, hi2 = 0;
    for (int i = 0; i < nFills_; i++) {
      if (!std::isnan(fillAvg_[i])) { lo = fminf(lo, fillAvg_[i]); hi = fmaxf(hi, fillAvg_[i]); }
      if (!std::isnan(fillCost100_[i])) { lo2 = fminf(lo2, fillCost100_[i]); hi2 = fmaxf(hi2, fillCost100_[i]); }
    }
    lo = std::floor(lo) - (hi - lo < 2 ? 1 : 0);
    hi = std::ceil(hi) + (hi - lo < 2 ? 1 : 0);
    if (hi <= lo) hi = lo + 2;
    lo2 = std::floor(lo2);
    hi2 = std::ceil(hi2);
    if (hi2 <= lo2) hi2 = lo2 + 2;
    auto sx = [&](int i) { return ox + T_X0 + 8 + (nFills_ > 1 ? i * (T_X1 - T_X0 - 16) / static_cast<float>(nFills_ - 1) : (T_X1 - T_X0 - 16) / 2.0f); };
    auto sy = [&](float v) { return oy + T_Y0 + T_H - clampf((v - lo) / (hi - lo), 0, 1) * T_H; };
    auto sy2 = [&](float v) { return oy + T_Y0 + T_H - clampf((v - lo2) / (hi2 - lo2), 0, 1) * T_H; };
    char t[24];
    for (int k = 0; k <= 2; k++) {
      const float v = lo + (hi - lo) * k / 2;
      const int32_t y = static_cast<int32_t>(sy(v));
      line(layer, ox + T_X0, y, ox + T_X1, y, theme::LINE, 1);
      fmt::number(t, sizeof(t), v, (hi - lo) < 4 ? 1 : 0);
      text(layer, t, ox, y - 6, T_X0 - 5, LV_TEXT_ALIGN_RIGHT, theme::ACCENT);
      const float v2 = lo2 + (hi2 - lo2) * k / 2;
      char n[12];
      fmt::number(n, sizeof(n), v2, 0);
      snprintf(t, sizeof(t), "%s " SYM_EURO, n);
      text(layer, t, ox + T_X1 + 4, static_cast<int32_t>(sy2(v2)) - 6, 30, LV_TEXT_ALIGN_LEFT, theme::MUTED);
    }
    float px[MAX_FILLS], py[MAX_FILLS];
    int n = 0;
    for (int i = 0; i < nFills_; i++)
      if (!std::isnan(fillCost100_[i])) { px[n] = sx(i); py[n] = sy2(fillCost100_[i]); n++; }
    smooth(layer, px, py, n, theme::MUTED, 1, true);
    n = 0;
    for (int i = 0; i < nFills_; i++)
      if (!std::isnan(fillAvg_[i])) { px[n] = sx(i); py[n] = sy(fillAvg_[i]); n++; }
    smooth(layer, px, py, n, theme::ACCENT, 2, false);
    for (int i = 0; i < n; i++) dot(layer, px[i], py[i], 3, theme::ACCENT);
    // Kacheln: letzte Füllung, Ø letzte, seit Füllung 1
    const int last = nFills_ - 1;
    char a[16], b[16], v[3][32];
    fmt::number(a, sizeof(a), fillL_[last], 1);
    fmt::number(b, sizeof(b), fillL_[last] * fillPrice_[last], 0);
    snprintf(v[0], sizeof(v[0]), "%s l " SYM_DOT " %s " SYM_EURO, a, b);
    fmt::number(a, sizeof(a), fillAvg_[last], 1);
    snprintf(v[1], sizeof(v[1]), "%s l/100", a);
    const float change = (nFills_ > 1 && fillAvg_[0] > 0) ? (fillAvg_[last] - fillAvg_[0]) / fillAvg_[0] * 100 : NAN;
    fmt::number(a, sizeof(a), std::fabs(change), 0);
    snprintf(v[2], sizeof(v[2]), std::isnan(change) ? "%s" : "%s%s %%", std::isnan(change) ? a : (change < 0 ? "\xE2\x80\x93" : "+"), a);
    if (std::isnan(change)) snprintf(v[2], sizeof(v[2]), "%s", fmt::NO_VALUE);
    static const char* const L[3] = {"Letzte Füllung", SYM_AVG " letzte", "Seit Füllung 1"};
    const int32_t w = (BOARD_LCD_HOR_RES - 20 - 12) / 3, y = oy + TOP + 150 - 4;
    for (int k = 0; k < 3; k++) {
      const int32_t x = ox + 10 + k * (w + 6);
      rect(layer, x, y, x + w, y + 32, theme::SURFACE, theme::RADIUS_TILE);
      text(layer, L[k], x + 7, y + 3, w - 8, LV_TEXT_ALIGN_LEFT, theme::MUTED);
      text(layer, v[k], x + 7, y + 15, w - 8, LV_TEXT_ALIGN_LEFT, k == 2 && change < 0 ? theme::GOOD : theme::TEXT, &font_m12);
    }
  }

  void drawStats(lv_layer_t* layer, int32_t ox, int32_t oy) {
    text(layer, SYM_AVG " l/100 km nach Strecke", ox + 12, oy + TOP + 4, 160, LV_TEXT_ALIGN_LEFT, theme::MUTED);
    static const char* const NAMES[4] = {"< 5 km", "5\xE2\x80\x93" "20", "20\xE2\x80\x93" "50", "> 50"};
    float v[4];
    char t[64];
    for (int b = 0; b < 4; b++) {
      v[b] = bucketKm_[b] > 0 ? bucketL_[b] / bucketKm_[b] * 100 : NAN;
      const int32_t x = ox + A_X0 + b * A_STEP;
      const int32_t hh = std::isnan(v[b]) ? 0 : static_cast<int32_t>(clampf(v[b], 0, 10) / 10 * A_H);
      const int32_t yb = oy + A_Y0 + A_H;
      if (hh > 0) rect(layer, x, yb - hh, x + A_BAR_W, yb, b == 0 ? theme::WARN : theme::ACCENT, 2);
      fmt::number(t, sizeof(t), v[b], 1);
      text(layer, t, x - 6, yb - hh - 14, A_BAR_W + 12, LV_TEXT_ALIGN_CENTER, theme::TEXT, &font_m12);
      text(layer, NAMES[b], x - 6, yb + 2, A_BAR_W + 12, LV_TEXT_ALIGN_CENTER, theme::MUTED);
    }
    // Satz: Kurzstrecken brauchen x % mehr als 5–20 km
    if (!std::isnan(v[0]) && !std::isnan(v[1]) && v[1] > 0) {
      char n[12];
      fmt::number(n, sizeof(n), (v[0] / v[1] - 1) * 100, 0);
      snprintf(t, sizeof(t), "Kurzstrecken unter 5 km brauchen %s %% mehr als 5" "\xE2\x80\x93" "20 km.", n);
      lv_draw_label_dsc_t d;
      lv_draw_label_dsc_init(&d);
      d.text = t;
      d.text_local = 1;
      d.font = &font_small;
      d.color = theme::c(theme::WARN);
      lv_area_t a = {ox + 12, oy + TOP + 120, ox + 160, oy + TOP + 160};
      lv_draw_label(layer, &d, &a);
    }
    // Rechts: Summen
    char n[16], kv[6][2][32];
    snprintf(kv[0][0], 32, "Letzte %d Fahrten", nTrips_);
    fmt::number(n, sizeof(n), totKm_, 0);
    snprintf(kv[0][1], 32, "%s km", n);
    snprintf(kv[1][0], 32, "Verbraucht");
    fmt::number(n, sizeof(n), totL_, 1);
    snprintf(kv[1][1], 32, "%s l", n);
    snprintf(kv[2][0], 32, "Kosten");
    fmt::number(n, sizeof(n), totCost_, 0);
    snprintf(kv[2][1], 32, "%s " SYM_EURO, n);
    snprintf(kv[3][0], 32, SYM_AVG);
    fmt::number(n, sizeof(n), totKm_ > 0 ? totL_ / totKm_ * 100 : NAN, 1);
    snprintf(kv[3][1], 32, "%s l/100", n);
    snprintf(kv[4][0], 32, "Eco-Score Trend");
    char a2[12];
    fmt::number(n, sizeof(n), trendPrev_, 0);
    fmt::number(a2, sizeof(a2), trend10_, 0);
    snprintf(kv[4][1], 32, "%s " "\xE2\x86\x92" " %s", n, a2);
    snprintf(kv[5][0], 32, "Beste Fahrt");
    fmt::number(n, sizeof(n), bestAvg_, 1);
    if (bestIdx_ >= 0) snprintf(kv[5][1], 32, "Nr. %u " SYM_DOT " %s l", bestNumber_, n);
    else snprintf(kv[5][1], 32, "%s", fmt::NO_VALUE);
    for (int i = 0; i < 6; i++) {
      const int32_t y = oy + TOP + 4 + i * 17;
      text(layer, kv[i][0], ox + 170, y, 90, LV_TEXT_ALIGN_LEFT, theme::MUTED);
      uint32_t c = theme::TEXT;
      if (i == 4 && !std::isnan(trend10_) && !std::isnan(trendPrev_)) c = trend10_ > trendPrev_ ? theme::GOOD : theme::WARN;
      text(layer, kv[i][1], ox + 230, y - 1, 80, LV_TEXT_ALIGN_RIGHT, c, &font_m12);
    }
    fmt::number(n, sizeof(n), cutShare10_ * 100, 0);
    snprintf(t, sizeof(t), "Schubanteil %s %% der Fahrzeit (letzte 10 Fahrten)", n);
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.text = t;
    d.text_local = 1;
    d.font = &font_small;
    d.color = theme::c(theme::MUTED);
    lv_area_t a = {ox + 170, oy + TOP + 110, ox + 310, oy + TOP + 140};
    lv_draw_label(layer, &d, &a);
    // Seit Profilanlage
    char b2[16];
    fmt::number(n, sizeof(n), snap_.avgProfile.get(snap_.now), 1);
    fmt::number(b2, sizeof(b2), snap_.odoKm.get(snap_.now), 0);
    snprintf(t, sizeof(t), "Seit Profilanlage: " SYM_AVG " %s l/100", n);
    text(layer, t, ox + 170, oy + TOP + 146, 145, LV_TEXT_ALIGN_LEFT, theme::MUTED);
  }

  void drawTempo(lv_layer_t* layer, int32_t ox, int32_t oy) {
    text(layer, "l/100 km je Tempo (km/h)", ox + 12, oy + TOP + 4, 200, LV_TEXT_ALIGN_LEFT, theme::MUTED);
    auto sx = [&](float v) { return ox + S_X0 + (v - 30) / 100 * (S_X1 - S_X0); };
    auto sy = [&](float l) { return oy + S_Y0 + S_H - clampf((l - S_LO) / (S_HI - S_LO), 0, 1) * S_H; };
    char t[48];
    for (int l = 4; l <= 8; l += 2) {
      const int32_t y = static_cast<int32_t>(sy(l));
      line(layer, ox + S_X0, y, ox + S_X1, y, theme::LINE, 1);
      snprintf(t, sizeof(t), "%d", l);
      text(layer, t, ox, y - 6, S_X0 - 5, LV_TEXT_ALIGN_RIGHT, theme::MUTED);
    }
    for (int v = 30; v <= 130; v += 20) {
      snprintf(t, sizeof(t), "%d", v);
      text(layer, t, static_cast<int32_t>(sx(v)) - 15, oy + S_Y0 + S_H + 2, 30, LV_TEXT_ALIGN_CENTER, theme::MUTED);
    }
    float px[cfg::TEMPO_CLASSES], py[cfg::TEMPO_CLASSES];
    int n = 0, best = -1;
    float bestL = 0, bestV = 0;
    for (int c = 0; c < cfg::TEMPO_CLASSES; c++) {
      const float v = cfg::TEMPO_FIRST_KMH + c * cfg::TEMPO_STEP_KMH;
      if (snap_.tempoKm[c] < cfg::TEMPO_MIN_KM) {
        text(layer, "?", static_cast<int32_t>(sx(v)) - 5, oy + S_Y0 + S_H - 14, 10, LV_TEXT_ALIGN_CENTER, theme::MUTED);
        continue;
      }
      const float l = snap_.tempoL[c] / snap_.tempoKm[c] * 100;
      px[n] = sx(v);
      py[n] = sy(l);
      if (best < 0 || l < bestL) {
        best = n;
        bestL = l;
        bestV = v;
      }
      n++;
    }
    smooth(layer, px, py, n, theme::ACCENT, 2, false);
    for (int i = 0; i < n; i++) dot(layer, px[i], py[i], i == best ? 5 : 3, i == best ? theme::GOOD : theme::ACCENT);
    const int32_t by = oy + TOP + 142;
    rect(layer, ox + 10, by, ox + BOARD_LCD_HOR_RES - 10, by + 24, theme::SURFACE, theme::RADIUS_TILE);
    if (best >= 0) {
      text(layer, "Am sparsamsten bei", ox + 20, by + 5, 150, LV_TEXT_ALIGN_LEFT, theme::TEXT, &font_m12);
      char l[12];
      fmt::number(l, sizeof(l), bestL, 1);
      snprintf(t, sizeof(t), "%d km/h " SYM_DOT " %s l", static_cast<int>(bestV), l);
      text(layer, t, ox + 150, by + 4, 150, LV_TEXT_ALIGN_RIGHT, theme::GOOD, &font_m14);
    } else {
      text(layer, "Noch zu wenig Daten (je Tempo mindestens 5 km)", ox + 20, by + 5, 280, LV_TEXT_ALIGN_LEFT, theme::MUTED, &font_m12);
    }
    text(layer, "Nur Konstantfahrt im höchsten Gang " SYM_DOT " ? = noch zu wenig Daten", ox + 12, oy + theme::CONTENT_H - 14, 300,
         LV_TEXT_ALIGN_LEFT, theme::MUTED);
  }

  // ---------- Bedienung ----------
  void showTab(int tab) {
    tab_ = tab;
    for (int i = 0; i < TABS; i++) {
      const bool on = i == tab;
      lv_obj_set_style_bg_color(chips_[i], theme::c(on ? theme::ACCENT : theme::BG), 0);
      lv_obj_set_style_border_color(chips_[i], theme::c(on ? theme::ACCENT : theme::LINE), 0);
      lv_obj_set_style_text_color(lv_obj_get_child(chips_[i], 0), theme::c(on ? theme::BG : theme::MUTED), 0);
    }
    lv_obj_invalidate(canvas_);
  }

  static void onTab(lv_event_t* e) {
    auto* p = static_cast<HistoryPage*>(historySelf());
    p->showTab(static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e))));
  }

  // Fahrten: Tippen auf einen Balken zeigt seine Details
  static void onCanvasTap(lv_event_t* e) {
    auto* p = static_cast<HistoryPage*>(lv_event_get_user_data(e));
    if (p->tab_ != 0 || p->nBars_ == 0) return;
    lv_point_t pt;
    lv_indev_get_point(lv_indev_active(), &pt);
    lv_area_t a;
    lv_obj_get_coords(p->canvas_, &a);
    const int32_t x = pt.x - a.x1 - F_X0, y = pt.y - a.y1;
    if (x < 0 || x >= F_W || y < F_Y0 || y > F_Y0 + F_H) return;
    const int i = static_cast<int>(x / (F_W / static_cast<float>(MAX_BARS)));
    if (i < p->nBars_) {
      p->sel_ = i == p->nBars_ - 1 ? -1 : i;
      lv_obj_invalidate(p->canvas_);
    }
  }

  static Page* historySelf();

  lv_obj_t* chips_[TABS] = {};
  lv_obj_t* canvas_ = nullptr;
  int tab_ = 0;
  int sel_ = -1;
  uint16_t loadedSeq_ = 0xFFFF;
  bool tempoShown_ = false;
  CarSnapshot snap_;
  // Daten
  TripBar bars_[MAX_BARS] = {};
  int nBars_ = 0, nTrips_ = 0;
  float totKm_ = 0, totL_ = 0, totCost_ = 0, totCutS_ = 0, totDurS_ = 0;
  float bucketKm_[4] = {}, bucketL_[4] = {};
  float trend10_ = NAN, trendPrev_ = NAN, cutShare10_ = NAN;
  int bestIdx_ = -1;
  float bestAvg_ = NAN;
  uint16_t bestNumber_ = 0;
  float fillAvg_[MAX_FILLS] = {}, fillCost100_[MAX_FILLS] = {}, fillL_[MAX_FILLS] = {}, fillPrice_[MAX_FILLS] = {};
  int nFills_ = 0;
};

}  // namespace

Page* historyPage() {
  static HistoryPage page;
  return &page;
}

Page* HistoryPage::historySelf() { return historyPage(); }
