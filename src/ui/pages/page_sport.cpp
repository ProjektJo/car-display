// Seite "Sport" (A10, U Seite 2): Drehzahlbogen 240° (0–6500 U/min, ab 5800 warn) mit Tempo und Gang,
// vier Live-Balken (Leistung, Beschleunigung, Gaspedal, Saugrohrdruck) und Live-Diagramm der letzten 30 s
// mit umschaltbarem Serienpaar. Positionen aus der Vorschau (pLeistung).
#include <cmath>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "page.h"
#include "ui/live.h"
#include "ui/symbols.h"
#include "ui/theme.h"
#include "ui/ui_prefs.h"
#include "ui/values.h"
#include "util/format.h"

namespace {

using live::Ser;

// Drehzahlbogen
constexpr int32_t CX = 78, CY = 66, R = 56, ARC_W = 9;
constexpr int32_t A0 = 150, SWEEP = 240;
// Balken rechts
constexpr int32_t BAR_X = 166, BAR_RIGHT = 10;
constexpr int32_t BAR_TOPS[4] = {8, 35, 62, 89};
constexpr int32_t BAR_H = 5;
// Kopfzeile und Diagramm
constexpr int32_t HEAD_Y = 118;
constexpr int32_t CH_X0 = 30, CH_X1 = 290, CH_Y0 = 138, CH_H = 70;
constexpr int DASH_PX = 3;

// Serienpaare (A10): Tempo + Leistung, Drehzahl + Gas, Beschleunigung + Leistung
constexpr Ser PAIRS[3][2] = {{Ser::Speed, Ser::Kw}, {Ser::Rpm, Ser::Pedal}, {Ser::Acc, Ser::Kw}};

float angleOf(float rpm) {
  float c = rpm / cfg::RPM_GAUGE_MAX;
  c = c < 0 ? 0 : (c > 1 ? 1 : c);
  return A0 + c * SWEEP;
}

void arc(lv_layer_t* layer, int32_t cx, int32_t cy, float a0, float a1, uint32_t color, lv_opa_t opa, bool rounded) {
  lv_draw_arc_dsc_t d;
  lv_draw_arc_dsc_init(&d);
  d.center.x = cx;
  d.center.y = cy;
  d.radius = R + ARC_W / 2;
  d.width = ARC_W;
  d.start_angle = static_cast<lv_value_precise_t>(a0);
  d.end_angle = static_cast<lv_value_precise_t>(a1);
  d.color = theme::c(color);
  d.opa = opa;
  d.rounded = rounded;
  lv_draw_arc(layer, &d);
}

void line(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color, int32_t w,
          lv_opa_t opa = LV_OPA_COVER) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.p1.x = x1;
  d.p1.y = y1;
  d.p2.x = x2;
  d.p2.y = y2;
  d.color = theme::c(color);
  d.width = w;
  d.opa = opa;
  lv_draw_line(layer, &d);
}

void text(lv_layer_t* layer, const char* t, int32_t x, int32_t y, int32_t w, lv_text_align_t al, uint32_t color,
          const lv_font_t* font) {
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

struct Bar {
  lv_obj_t* label = nullptr;
  lv_obj_t* value = nullptr;
  lv_obj_t* track = nullptr;
  lv_obj_t* fill = nullptr;
  bool bipolar = false;
  char shown[32] = "";
  int32_t shownX = -1, shownW = -1;
  uint32_t shownColor = 0;
};

class SportPage : public Page {
 public:
  SportPage() : Page("Sport") {}

  void create(lv_obj_t* parent) override {
    gauge_ = lv_obj_create(parent);
    lv_obj_remove_style_all(gauge_);
    lv_obj_set_size(gauge_, BOARD_LCD_HOR_RES, theme::CONTENT_H);
    lv_obj_remove_flag(gauge_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(gauge_, onDraw, LV_EVENT_DRAW_MAIN, this);

    // Tempo und Gang in der Mitte des Bogens
    speed_ = theme::label(parent, &font_m28, false, fmt::NO_VALUE);
    lv_obj_set_width(speed_, 2 * R);
    lv_obj_set_style_text_align(speed_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(speed_, CX - R, CY - 24);
    lv_obj_t* u = theme::label(parent, &lv_font_montserrat_10, true, "km/h");
    lv_obj_set_width(u, 2 * R);
    lv_obj_set_style_text_align(u, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(u, CX - R, CY + 9);
    gearRow_ = lv_obj_create(parent);
    lv_obj_remove_style_all(gearRow_);
    lv_obj_set_size(gearRow_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(gearRow_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gearRow_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(gearRow_, 3, 0);
    lv_obj_remove_flag(gearRow_, LV_OBJ_FLAG_CLICKABLE);
    arrow_ = theme::label(gearRow_, &font_m14, false, SYM_UP);
    lv_obj_set_style_text_color(arrow_, theme::c(theme::GOOD), 0);
    lv_obj_add_flag(arrow_, LV_OBJ_FLAG_HIDDEN);
    gear_ = theme::label(gearRow_, &font_m14, false, fmt::NO_VALUE);
    lv_obj_t* gl = theme::label(gearRow_, &font_m12, true, "Gang");
    lv_obj_set_style_pad_bottom(gl, 1, 0);
    lv_obj_align(gearRow_, LV_ALIGN_TOP_MID, CX - BOARD_LCD_HOR_RES / 2, CY + 26);

    static const char* const LABELS[4] = {"Leistung", "Beschleunigung", "Gaspedal", "Saugrohrdruck"};
    for (int i = 0; i < 4; i++) makeBar(parent, i, LABELS[i], i == 1);

    head_ = theme::label(parent, &font_m12, true, "Letzte 30 s");
    lv_obj_set_pos(head_, 10, HEAD_Y);
    chip_ = lv_obj_create(parent);
    lv_obj_remove_style_all(chip_);
    lv_obj_set_size(chip_, LV_SIZE_CONTENT, 18);
    lv_obj_set_style_radius(chip_, 9, 0);
    lv_obj_set_style_border_width(chip_, 1, 0);
    lv_obj_set_style_border_color(chip_, theme::c(theme::LINE), 0);
    lv_obj_set_style_pad_hor(chip_, 8, 0);
    lv_obj_set_flex_flow(chip_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(chip_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(chip_, 6, 0);
    lv_obj_add_flag(chip_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(chip_, 6);
    lv_obj_add_event_cb(chip_, onChip, LV_EVENT_SHORT_CLICKED, this);
    chipA_ = theme::label(chip_, &font_m12, false, "");
    lv_obj_set_style_text_color(chipA_, theme::c(theme::ACCENT), 0);
    chipB_ = theme::label(chip_, &font_m12, true, "");
    theme::label(chip_, &font_m12, true, "\xEF\x81\x94");  // Pfeil rechts
    lv_obj_align(chip_, LV_ALIGN_TOP_RIGHT, -10, HEAD_Y - 2);
    refreshChip();
  }

  void onShow() override {
    refreshChip();
    lv_obj_invalidate(gauge_);
  }

  void tick(const CarSnapshot& s) override {
    if (s.shiftAdvice) {
      if (!shiftSince_) shiftSince_ = s.now ? s.now : 1;
    } else {
      shiftSince_ = 0;
    }
  }

  void update(const CarSnapshot& s) override {
    last_ = s;
    char t[32];
    fmt::number(t, sizeof(t), s.speed.get(s.now), 0);
    setText(speed_, shownSpeed_, sizeof(shownSpeed_), t);
    values::text(values::Key::Gear, s, t, sizeof(t));
    setText(gear_, shownGear_, sizeof(shownGear_), t);
    const bool arrow = shiftSince_ && s.now - shiftSince_ >= cfg::SHIFT_ARROW_DELAY_MS && !s.fuelCut;
    if (arrow != shownArrow_) {
      shownArrow_ = arrow;
      if (arrow) lv_obj_remove_flag(arrow_, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(arrow_, LV_OBJ_FLAG_HIDDEN);
    }

    // Balken
    const float kw = s.powerKw.get(s.now);
    char a[16], b[16];
    fmt::number(a, sizeof(a), kw, 0);
    fmt::number(b, sizeof(b), std::isnan(kw) ? NAN : kw * cfg::KW_TO_PS, 0);
    snprintf(t, sizeof(t), std::isnan(kw) ? "%s" : "%s kW " SYM_DOT " %s PS", a, b);
    setBar(0, t, std::isnan(kw) ? 0 : kw / (s.profile.powerKw > 0 ? s.profile.powerKw : 55), theme::ACCENT);
    const float acc = s.accel.get(s.now);
    fmt::number(a, sizeof(a), std::isnan(acc) ? NAN : std::fabs(acc), 1);
    if (std::isnan(acc))
      snprintf(t, sizeof(t), "%s", a);
    else
      snprintf(t, sizeof(t), "%s%s m/s" "\xC2\xB2", acc >= 0 ? "+" : "\xE2\x80\x93", a);  // – und ²
    setBar(1, t, std::isnan(acc) ? 0 : acc / 4.0f, (!std::isnan(acc) && acc < 0) ? theme::MUTED : theme::ACCENT);
    const float pedal = values::value(values::Key::Pedal, s);
    fmt::number(a, sizeof(a), pedal, 0);
    snprintf(t, sizeof(t), std::isnan(pedal) ? "%s" : "%s %%", a);
    setBar(2, t, std::isnan(pedal) ? 0 : pedal / 100.0f, theme::ACCENT);
    const float map = s.map.get(s.now);
    fmt::number(a, sizeof(a), map, 0);
    snprintf(t, sizeof(t), std::isnan(map) ? "%s" : "%s kPa", a);
    setBar(3, t, std::isnan(map) ? 0 : map / 100.0f, theme::ACCENT);

    // Kopfzeile: laufende Sprintmessung
    const bool running = s.sprint.state == perf::State::Running;
    if (running) {
      fmt::number(a, sizeof(a), s.sprint.elapsed, 1);
      snprintf(t, sizeof(t), "0" "\xE2\x80\x93" "100 läuft " SYM_DOT " %s s", a);
    } else {
      snprintf(t, sizeof(t), "Letzte 30 s");
    }
    setText(head_, shownHead_, sizeof(shownHead_), t);
    if (running != shownRunning_) {
      shownRunning_ = running;
      lv_obj_set_style_text_color(head_, theme::c(running ? theme::ACCENT : theme::MUTED), 0);
    }

    // Bogen und Diagramm neu zeichnen, wenn sich Drehzahl oder der Puffer geändert haben (höchstens 5 Hz)
    const float rpm = s.rpm.get(s.now);
    const int rpmStep = std::isnan(rpm) ? -1 : static_cast<int>(rpm / 25);
    if ((rpmStep != drawnRpm_ || live::seq() != drawnSeq_) && s.now - lastDraw_ >= cfg::CHART_MIN_REDRAW_MS) {
      drawnRpm_ = rpmStep;
      drawnSeq_ = live::seq();
      lastDraw_ = s.now;
      lv_obj_invalidate(gauge_);
    }
  }

 private:
  void makeBar(lv_obj_t* parent, int i, const char* label, bool bipolar) {
    Bar& b = bars_[i];
    b.bipolar = bipolar;
    const int32_t w = BOARD_LCD_HOR_RES - BAR_X - BAR_RIGHT;
    b.label = theme::label(parent, &font_small, true, label);
    lv_obj_set_pos(b.label, BAR_X, BAR_TOPS[i]);
    b.value = theme::label(parent, &font_m12, false, fmt::NO_VALUE);
    lv_obj_set_width(b.value, w);
    lv_obj_set_style_text_align(b.value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(b.value, BAR_X, BAR_TOPS[i]);
    b.track = lv_obj_create(parent);
    lv_obj_remove_style_all(b.track);
    lv_obj_set_pos(b.track, BAR_X, BAR_TOPS[i] + 17);
    lv_obj_set_size(b.track, w, BAR_H);
    lv_obj_set_style_radius(b.track, 3, 0);
    lv_obj_set_style_bg_color(b.track, theme::c(theme::SURFACE), 0);
    lv_obj_set_style_bg_opa(b.track, LV_OPA_COVER, 0);
    lv_obj_remove_flag(b.track, LV_OBJ_FLAG_CLICKABLE);
    if (bipolar) {
      lv_obj_t* mid = lv_obj_create(parent);
      lv_obj_remove_style_all(mid);
      lv_obj_set_pos(mid, BAR_X + w / 2, BAR_TOPS[i] + 15);
      lv_obj_set_size(mid, 1, BAR_H + 4);
      lv_obj_set_style_bg_color(mid, theme::c(theme::MUTED), 0);
      lv_obj_set_style_bg_opa(mid, LV_OPA_COVER, 0);
    }
    b.fill = lv_obj_create(b.track);
    lv_obj_remove_style_all(b.fill);
    lv_obj_set_size(b.fill, 0, BAR_H);
    lv_obj_set_style_radius(b.fill, 3, 0);
    lv_obj_set_style_bg_opa(b.fill, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b.fill, theme::c(theme::ACCENT), 0);
    b.shownColor = theme::ACCENT;
  }

  // frac 0–1 bzw. bei der Beschleunigung −1…+1 um die Mitte
  void setBar(int i, const char* text, float frac, uint32_t color) {
    Bar& b = bars_[i];
    setText(b.value, b.shown, sizeof(b.shown), text);
    const int32_t w = BOARD_LCD_HOR_RES - BAR_X - BAR_RIGHT;
    int32_t x = 0, fw = 0;
    if (b.bipolar) {
      const float f = frac < -1 ? -1 : (frac > 1 ? 1 : frac);
      fw = static_cast<int32_t>(std::lround(std::fabs(f) * w / 2));
      x = f < 0 ? w / 2 - fw : w / 2;
    } else {
      const float f = frac < 0 ? 0 : (frac > 1 ? 1 : frac);
      fw = static_cast<int32_t>(std::lround(f * w));
    }
    if (x != b.shownX || fw != b.shownW) {
      b.shownX = x;
      b.shownW = fw;
      lv_obj_set_pos(b.fill, x, 0);
      lv_obj_set_width(b.fill, fw);
    }
    if (color != b.shownColor) {
      b.shownColor = color;
      lv_obj_set_style_bg_color(b.fill, theme::c(color), 0);
    }
  }

  static void setText(lv_obj_t* l, char* shown, size_t size, const char* t) {
    if (strcmp(shown, t) == 0) return;
    snprintf(shown, size, "%s", t);
    lv_label_set_text(l, t);
  }

  static int pair() {
    const int p = uiprefs::get().sportPair;
    return p < 3 ? p : 0;
  }

  void refreshChip() {
    char t[32];
    snprintf(t, sizeof(t), "\xE2\x80\x94 %s", live::label(PAIRS[pair()][0]));  // —
    lv_label_set_text(chipA_, t);
    snprintf(t, sizeof(t), "- - %s", live::label(PAIRS[pair()][1]));
    lv_label_set_text(chipB_, t);
    lv_obj_invalidate(gauge_);
  }

  static void onChip(lv_event_t* e) {
    auto* p = static_cast<SportPage*>(lv_event_get_user_data(e));
    uiprefs::get().sportPair = static_cast<uint8_t>((pair() + 1) % 3);
    uiprefs::save();
    p->refreshChip();
  }

  static void onDraw(lv_event_t* e) {
    static_cast<SportPage*>(lv_event_get_user_data(e))->draw(lv_event_get_layer(e), lv_event_get_target_obj(e));
  }

  void draw(lv_layer_t* layer, lv_obj_t* obj) {
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    const int32_t cx = a.x1 + CX, cy = a.y1 + CY;
    const CarSnapshot& s = last_;
    // Drehzahlbogen: Grund, Rotbereich (warn, schwach), Zeiger
    arc(layer, cx, cy, A0, A0 + SWEEP, theme::SURFACE, LV_OPA_COVER, true);
    arc(layer, cx, cy, angleOf(cfg::RPM_GAUGE_WARN), A0 + SWEEP, theme::WARN, 90, false);
    const float rpm = s.rpm.get(s.now);
    if (!std::isnan(rpm) && rpm > 0)
      arc(layer, cx, cy, A0, angleOf(rpm) + 0.5f, rpm > cfg::RPM_GAUGE_WARN ? theme::WARN : theme::ACCENT, LV_OPA_COVER, true);
    // Skala ×1000
    for (int k = 0; k <= 6; k++) {
      const float ang = angleOf(k * 1000.0f) * 3.14159265f / 180.0f;
      const float c = std::cos(ang), sn = std::sin(ang);
      line(layer, cx + std::lround((R - 6) * c), cy + std::lround((R - 6) * sn), cx + std::lround((R + 5) * c),
           cy + std::lround((R + 5) * sn), theme::BG, 2);
      char t[4];
      snprintf(t, sizeof(t), "%d", k);
      text(layer, t, cx + std::lround((R - 15) * c) - 6, cy + std::lround((R - 15) * sn) - 6, 12, LV_TEXT_ALIGN_CENTER,
           theme::MUTED, &lv_font_montserrat_10);
    }

    // Live-Diagramm 30 s
    const int32_t x0 = a.x1 + CH_X0, x1 = a.x1 + CH_X1, y0 = a.y1 + CH_Y0;
    for (int f = 0; f <= 2; f++) line(layer, x0, y0 + CH_H * f / 2, x1, y0 + CH_H * f / 2, theme::LINE, 1);
    const Ser* pr = PAIRS[pair()];
    if (pr[0] == Ser::Acc) {
      const int32_t yz = y0 + CH_H - static_cast<int32_t>((0 - live::lo(Ser::Acc)) / (live::hi(Ser::Acc) - live::lo(Ser::Acc)) * CH_H);
      line(layer, x0, yz, x1, yz, theme::MUTED, 1, 150);
    }
    const int n = live::count();
    const uint32_t tNow = n ? live::at(n - 1).t : 0;
    for (int idx = 0; idx < 2; idx++) {
      const Ser r = pr[idx];
      const uint32_t col = idx ? theme::MUTED : theme::ACCENT;
      bool pen = false;
      int32_t px = 0, py = 0;
      for (int i = 0; i < n; i++) {
        const live::Entry& e = live::at(i);
        const float age = (tNow - e.t) / 1000.0f;
        if (age > cfg::LIVE_WINDOW_S) continue;
        const float v = e.v[static_cast<int>(r)];
        if (std::isnan(v)) {
          pen = false;
          continue;
        }
        float c = (v - live::lo(r)) / (live::hi(r) - live::lo(r));
        c = c < 0 ? 0 : (c > 1 ? 1 : c);
        const int32_t x = x1 - static_cast<int32_t>(std::lround(age / cfg::LIVE_WINDOW_S * (x1 - x0)));
        const int32_t y = y0 + CH_H - static_cast<int32_t>(std::lround(c * CH_H));
        if (pen && (idx == 0 || ((x - x0) / DASH_PX) % 2 == 0)) line(layer, px, py, x, y, col, 2);
        px = x;
        py = y;
        pen = true;
      }
      char t[12];
      fmt::number(t, sizeof(t), live::hi(r), 0);
      const int32_t ax = idx ? x1 + 3 : x0 - 3 - 28;
      const lv_text_align_t al = idx ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_RIGHT;
      text(layer, t, ax, y0 - 5, 28, al, col, &lv_font_montserrat_10);
      fmt::number(t, sizeof(t), live::lo(r), 0);
      text(layer, t, ax, y0 + CH_H - 7, 28, al, col, &lv_font_montserrat_10);
    }
  }

  lv_obj_t* gauge_ = nullptr;
  lv_obj_t* speed_ = nullptr;
  lv_obj_t* gearRow_ = nullptr;
  lv_obj_t* arrow_ = nullptr;
  lv_obj_t* gear_ = nullptr;
  lv_obj_t* head_ = nullptr;
  lv_obj_t* chip_ = nullptr;
  lv_obj_t* chipA_ = nullptr;
  lv_obj_t* chipB_ = nullptr;
  Bar bars_[4];
  char shownSpeed_[12] = "";
  char shownGear_[8] = "";
  char shownHead_[40] = "";
  bool shownArrow_ = false;
  bool shownRunning_ = false;
  uint32_t shiftSince_ = 0;
  int drawnRpm_ = -2;
  uint32_t drawnSeq_ = 0;
  uint32_t lastDraw_ = 0;
  CarSnapshot last_;
};

}  // namespace

Page* sportPage() {
  static SportPage page;
  return &page;
}
