#include "linechart.h"

#include <cmath>
#include <cstdio>

#include "ui/history.h"
#include "ui/theme.h"
#include "util/format.h"

namespace linechart {

namespace {

constexpr int DASH_PX = 3;          // gestrichelt: 3 px Strich, 3 px Lücke
constexpr int32_t AXIS_GAP = 3;
constexpr int32_t AXIS_W = 28;
constexpr int32_t LINE_W = 2;
constexpr float STEPS[] = {0.5f, 1, 2, 5, 10, 20, 25, 50, 100, 250, 500, 1000};

void line(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color, int32_t w) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.p1.x = x1;
  d.p1.y = y1;
  d.p2.x = x2;
  d.p2.y = y2;
  d.color = theme::c(color);
  d.width = w;
  d.round_start = d.round_end = w > 1;
  lv_draw_line(layer, &d);
}

void text(lv_layer_t* layer, const char* t, int32_t x, int32_t y, int32_t w, lv_text_align_t align, uint32_t color) {
  lv_draw_label_dsc_t d;
  lv_draw_label_dsc_init(&d);
  d.text = t;
  d.text_local = 1;
  d.font = &font_m12;
  d.color = theme::c(color);
  d.align = align;
  lv_area_t a = {x, y, x + w - 1, y + 14};
  lv_draw_label(layer, &d, &a);
}

// Runde Achse (Vorschau): Schrittweite aus einer festen Liste, lo/hi auf Vielfache davon
void niceAxis(values::Series r, float& lo, float& hi) {
  if (values::seriesFromZero(r)) lo = 0;
  if (r == values::Series::Inst && hi < 10) hi = 10;
  if (hi - lo < 1) {
    hi += 0.5f;
    lo -= 0.5f;
  }
  const float raw = (hi - lo) / 2 * 0.96f;
  float st = 1000;
  for (float s : STEPS)
    if (s >= raw) {
      st = s;
      break;
    }
  lo = std::floor(lo / st + 0.02f) * st;
  hi = std::ceil(hi / st - 0.02f) * st;
  if (hi - lo < 2 * st) hi = lo + 2 * st;
}

}  // namespace

bool minMax(values::Series r, int windowS, float& lo, float& hi) {
  bool any = false;
  const int n = history::count() < windowS ? history::count() : windowS;
  for (int a = 0; a < n; a++) {
    const float v = history::at(r, a);
    if (std::isnan(v)) continue;
    if (!any || v < lo) lo = v;
    if (!any || v > hi) hi = v;
    any = true;
  }
  return any;
}

void draw(lv_layer_t* layer, lv_obj_t* obj, const Spec& spec) {
  lv_area_t a;
  lv_obj_get_coords(obj, &a);
  const int32_t x0 = a.x1 + spec.x, y0 = a.y1 + spec.y, w = spec.w, h = spec.h;
  for (int f = 0; f <= 2; f++) line(layer, x0, y0 + h * f / 2, x0 + w, y0 + h * f / 2, theme::LINE, 1);

  const int win = spec.windowS > 1 ? spec.windowS : 2;
  const int n = history::count() < win ? history::count() : win;
  const float step = static_cast<float>(w) / (win - 1);
  // Höchstens ein Punkt je Pixelspalte
  const int every = step >= 1 ? 1 : static_cast<int>(std::ceil(1.0f / step));

  for (int idx = 0; idx < 2; idx++) {
    const values::Series r = spec.series[idx];
    if (r == values::Series::None) continue;
    float lo = 0, hi = 0;
    if (!minMax(r, win, lo, hi)) continue;
    niceAxis(r, lo, hi);
    const uint32_t col = idx == 0 ? theme::ACCENT : theme::MUTED;
    bool pen = false;
    int32_t px = 0, py = 0;
    for (int age = ((n - 1) / every) * every; age >= 0; age -= every) {
      const float v = history::at(r, age);
      if (std::isnan(v)) {
        pen = false;
        continue;
      }
      const int32_t x = x0 + static_cast<int32_t>(std::lround((win - 1 - age) * step));
      float c = (v - lo) / (hi - lo);
      c = c < 0 ? 0 : (c > 1 ? 1 : c);
      const int32_t y = y0 + h - static_cast<int32_t>(std::lround(c * h));
      // zweite Reihe gestrichelt: nur jeden zweiten 3-px-Abschnitt zeichnen
      if (pen && (idx == 0 || ((x - x0) / DASH_PX) % 2 == 0)) line(layer, px, py, x, y, col, LINE_W);
      px = x;
      py = y;
      pen = true;
    }
    if (!spec.axes) continue;
    const int dec = r == values::Series::Volt ? 1 : 0;  // Mitte der Spannungsachse mit Nachkommastelle
    char t[16];
    const int32_t ax = idx == 0 ? x0 - AXIS_GAP - AXIS_W : x0 + w + AXIS_GAP;
    const lv_text_align_t al = idx == 0 ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT;
    fmt::number(t, sizeof(t), hi, 0);
    text(layer, t, ax, y0 - 6, AXIS_W, al, col);
    fmt::number(t, sizeof(t), (hi + lo) / 2, dec);
    text(layer, t, ax, y0 + h / 2 - 7, AXIS_W, al, col);
    fmt::number(t, sizeof(t), lo, 0);
    text(layer, t, ax, y0 + h - 8, AXIS_W, al, col);
  }
}

}  // namespace linechart
