// Seite "G-Kraft" (U Seite 8, nur mit MPU6050): Kreis mit Punkt für Quer- und Längsbeschleunigung,
// Spitzenwerte quer, beschleunigen und bremsen. Positionen aus der Vorschau (pGkraft).
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "config.h"
#include "page.h"
#include "ui/theme.h"
#include "util/format.h"

namespace {

constexpr int32_t CX = 100, CY = 106, R = 84;
constexpr int32_t DOT_R = 7;
constexpr int32_t COL_X = 204, COL_Y = 20, ROW_STEP = 44;

void line(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.p1.x = x1;
  d.p1.y = y1;
  d.p2.x = x2;
  d.p2.y = y2;
  d.color = theme::c(theme::LINE);
  d.width = 1;
  lv_draw_line(layer, &d);
}

void circle(lv_layer_t* layer, int32_t cx, int32_t cy, int32_t r, uint32_t color, bool fill) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.radius = LV_RADIUS_CIRCLE;
  if (fill) {
    d.bg_color = theme::c(color);
    d.bg_opa = LV_OPA_COVER;
  } else {
    d.bg_opa = LV_OPA_TRANSP;
    d.border_color = theme::c(color);
    d.border_width = 1;
    d.border_opa = LV_OPA_COVER;
  }
  lv_area_t a = {cx - r, cy - r, cx + r, cy + r};
  lv_draw_rect(layer, &d, &a);
}

void text(lv_layer_t* layer, const char* t, int32_t x, int32_t y) {
  lv_draw_label_dsc_t d;
  lv_draw_label_dsc_init(&d);
  d.text = t;
  d.text_local = 1;
  d.font = &font_small;
  d.color = theme::c(theme::MUTED);
  lv_area_t a = {x, y, x + 40, y + 12};
  lv_draw_label(layer, &d, &a);
}

class GforcePage : public Page {
 public:
  GforcePage() : Page("G-Kraft") {}

  bool available(const CarSnapshot& s) const override { return s.hasImu; }

  void create(lv_obj_t* parent) override {
    disc_ = lv_obj_create(parent);
    lv_obj_remove_style_all(disc_);
    lv_obj_set_size(disc_, 200, theme::CONTENT_H);
    lv_obj_remove_flag(disc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(disc_, onDraw, LV_EVENT_DRAW_MAIN, this);
    static const char* const LABELS[3] = {"Quer max", "Beschleunigen max", "Bremsen max"};
    for (int i = 0; i < 3; i++) {
      lv_obj_t* l = theme::label(parent, &font_small, true, LABELS[i]);
      lv_obj_set_pos(l, COL_X, COL_Y + i * ROW_STEP);
      peak_[i] = theme::label(parent, &font_m20, false, fmt::NO_VALUE);
      lv_obj_set_pos(peak_[i], COL_X, COL_Y + i * ROW_STEP + 13);
    }
    lv_obj_t* l = theme::label(parent, &font_small, true, "Jetzt");
    lv_obj_set_pos(l, COL_X, COL_Y + 3 * ROW_STEP);
    now_ = theme::label(parent, &font_m12, false, fmt::NO_VALUE);
    lv_obj_set_pos(now_, COL_X, COL_Y + 3 * ROW_STEP + 13);
    hint_ = theme::label(parent, &font_small, true, "");
    lv_obj_set_pos(hint_, COL_X, COL_Y + 4 * ROW_STEP + 6);
    lv_obj_set_width(hint_, BOARD_LCD_HOR_RES - COL_X - 6);
    lv_label_set_long_mode(hint_, LV_LABEL_LONG_WRAP);
  }

  // Spitzenwerte auch sammeln, wenn die Seite nicht sichtbar ist
  void tick(const CarSnapshot& s) override {
    const float lat = s.imuLat.get(s.now), lon = s.imuLong.get(s.now);
    if (std::isnan(lat) || std::isnan(lon)) return;
    latG_ = lat / cfg::GRAVITY;
    lonG_ = lon / cfg::GRAVITY;
    peakLat_ = std::fmax(peakLat_, std::fabs(latG_));
    peakAcc_ = std::fmax(peakAcc_, lonG_);
    peakBrk_ = std::fmax(peakBrk_, -lonG_);
  }

  void update(const CarSnapshot& s) override {
    const float vals[3] = {peakLat_, peakAcc_, peakBrk_};
    char t[24];
    for (int i = 0; i < 3; i++) {
      char n[12];
      fmt::number(n, sizeof(n), vals[i], 2);
      snprintf(t, sizeof(t), "%s g", n);
      if (strcmp(t, shown_[i]) != 0) {
        snprintf(shown_[i], sizeof(shown_[i]), "%s", t);
        lv_label_set_text(peak_[i], t);
      }
    }
    char n[12];
    fmt::number(n, sizeof(n), std::hypot(latG_, lonG_), 2);
    snprintf(t, sizeof(t), "%s g", n);
    lv_label_set_text(now_, t);
    lv_label_set_text_static(hint_, s.imuReady ? "" : "Einbaulage wird gelernt: kurz stehen, dann geradeaus anfahren");
    if (s.now - lastDraw_ >= cfg::CHART_MIN_REDRAW_MS) {
      lastDraw_ = s.now;
      lv_obj_invalidate(disc_);
    }
  }

 private:
  static void onDraw(lv_event_t* e) {
    auto* p = static_cast<GforcePage*>(lv_event_get_user_data(e));
    lv_area_t a;
    lv_obj_get_coords(lv_event_get_target_obj(e), &a);
    lv_layer_t* layer = lv_event_get_layer(e);
    const int32_t cx = a.x1 + CX, cy = a.y1 + CY;
    const float sc = R / cfg::G_FORCE_RANGE;
    for (float g : {0.2f, 0.4f, 0.6f}) circle(layer, cx, cy, static_cast<int32_t>(g * sc), theme::LINE, false);
    line(layer, cx - R, cy, cx + R, cy);
    line(layer, cx, cy - R, cx, cy + R);
    text(layer, "0,2 g", cx + static_cast<int32_t>(0.2f * sc) + 2, cy - 13);
    text(layer, "0,4", cx + static_cast<int32_t>(0.4f * sc) + 2, cy - 13);
    // Punkt: quer nach rechts = rechts, bremsen = nach vorn (oben)
    auto clampG = [](float g) { return g < -cfg::G_FORCE_RANGE ? -cfg::G_FORCE_RANGE : (g > cfg::G_FORCE_RANGE ? cfg::G_FORCE_RANGE : g); };
    const int32_t dx = static_cast<int32_t>(std::lround(clampG(-p->latG_) * sc));
    const int32_t dy = static_cast<int32_t>(std::lround(clampG(p->lonG_) * sc));
    circle(layer, cx + dx, cy + dy, DOT_R, theme::ACCENT, true);
  }

  lv_obj_t* disc_ = nullptr;
  lv_obj_t* peak_[3] = {};
  lv_obj_t* now_ = nullptr;
  lv_obj_t* hint_ = nullptr;
  char shown_[3][24] = {};
  float latG_ = 0, lonG_ = 0;
  float peakLat_ = 0, peakAcc_ = 0, peakBrk_ = 0;
  uint32_t lastDraw_ = 0;
};

}  // namespace

Page* gforcePage() {
  static GforcePage page;
  return &page;
}
