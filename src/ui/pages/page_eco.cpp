// Seite "Eco" (Startseite, U Seite 1): Momentanverbrauch, Eco-Kurve, Gang mit Schaltpfeil,
// Hinweis-Feld mit den Spartipps (A9) und die untere Leiste Eco-Score, Schub gespart, Gebremst.
// Positionen aus der Vorschau (pEco), ein CSS-px = ein Display-Pixel.
#include <Arduino.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "calc/eco.h"
#include "config.h"
#include "hw/touch.h"
#include "page.h"
#include "ui/overlay.h"
#include "ui/symbols.h"
#include "ui/theme.h"
#include "ui/ui_prefs.h"
#include "util/format.h"

namespace {

// Kopf
constexpr int32_t PAD = 12;
constexpr int32_t BIG_TOP = 18;
constexpr int32_t UNIT_GAP = 3;
constexpr int32_t GEAR_TOP = 8;
// Hinweis-Feld links neben dem Gang (U: 118 × 40 px, rechts 58 px, oben 10 px)
constexpr int32_t HINT_W = 118, HINT_H = 40, HINT_RIGHT = 58, HINT_TOP = 10;
constexpr int32_t PILL_H = 18;
// Kurve (Vorschau: x0 26, x1 300, y0 64, Höhe 86)
constexpr int32_t CX0 = 26, CX1 = 300, CY0 = 64, CH = 86;
constexpr int32_t POINTS = 5;
constexpr int32_t CURVE_W = 2;
constexpr int32_t DOT_R = 4, DOT_R_NOW = 5;
constexpr int32_t TRIP_R = 5;          // Fahrt-Punkt: Ring
constexpr int32_t TRIP_LABEL_GAP = 30; // feste Achsen-Beschriftung näher als das: ausblenden
constexpr int32_t GRID_STEP = 4;      // Raster bei 0, 4, 8, 12 l/100 km
constexpr lv_opa_t FILL_OPA = 46;     // schwache Flächenfüllung unter der Kurve (≈ 0,18)
constexpr lv_opa_t REF_OPA = 180;     // Bezugslinie Tank-Schnitt (Vorschau: 0,7)
constexpr int32_t SAMPLES_PER_SEG = 12;
// Untere Leiste: drei Kacheln, 8 px Rand, 6 px Abstand, 6 px vom unteren Rand
constexpr int32_t BAR_SIDE = 8, BAR_GAP = 6, BAR_BOTTOM = 6, BAR_H = 36;
constexpr int32_t BAR_PAD_HOR = 8, BAR_PAD_VER = 3;

const char* const X_LABELS[POINTS] = {"Tank", "100 km", "10 km", "1 km", "Momentan"};

// Momentanverbrauch in Montserrat 38 (LVGL, nur ASCII); fehlende Zeichen wie "–" aus font_m28
lv_font_t font38;

float sx(float i) { return CX0 + 20 + i * ((CX1 - CX0 - 40) / 4.0f); }
float sy(float v) {
  const float c = v < 0 ? 0 : (v > cfg::ECO_CURVE_TOP_L100 ? cfg::ECO_CURVE_TOP_L100 : v);
  return CY0 + CH - c / cfg::ECO_CURVE_TOP_L100 * CH;
}

// Farbe nach Bezug (Spar-Ziel, sonst Tank-Schnitt): good unter 95 %, warn über 110 % (M, A9)
uint32_t refColor(float v, float ref) {
  if (std::isnan(v) || std::isnan(ref) || ref <= 0) return theme::TEXT;
  if (v < ref * cfg::COLOR_GOOD_BELOW) return theme::GOOD;
  if (v > ref * cfg::COLOR_WARN_ABOVE) return theme::WARN;
  return theme::TEXT;
}

uint32_t scoreColor(float s) {
  if (std::isnan(s)) return theme::TEXT;
  return s >= cfg::SCORE_GOOD ? theme::GOOD : (s < cfg::SCORE_OK ? theme::WARN : theme::TEXT);
}

struct Curve {
  float v[POINTS];      // NAN = kein Punkt
  float ref;            // Bezug für die Farben
  float goal;           // Spar-Ziel, NAN = aus
  float tank;           // Tank-Schnitt (Bezugslinie ohne Ziel)
  float tripV = NAN;    // Ø Fahrt
  float tripPos = NAN;  // Lage auf der Achse (eco::tripAxisPos), NAN = kein Fahrt-Punkt
};

bool sameCurve(const Curve& a, const Curve& b) {
  auto eq = [](float x, float y) { return (std::isnan(x) && std::isnan(y)) || std::fabs(x - y) < 0.05f; };
  for (int i = 0; i < POINTS; i++)
    if (!eq(a.v[i], b.v[i])) return false;
  return eq(a.ref, b.ref) && eq(a.goal, b.goal) && eq(a.tank, b.tank) && eq(a.tripV, b.tripV) &&
         ((std::isnan(a.tripPos) && std::isnan(b.tripPos)) || std::fabs(a.tripPos - b.tripPos) < 0.01f);
}

void drawLine(lv_layer_t* layer, float x1, float y1, float x2, float y2, uint32_t color, int32_t w, lv_opa_t opa = LV_OPA_COVER,
              int32_t dash = 0, int32_t gap = 0) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.p1.x = static_cast<lv_value_precise_t>(std::lround(x1));
  d.p1.y = static_cast<lv_value_precise_t>(std::lround(y1));
  d.p2.x = static_cast<lv_value_precise_t>(std::lround(x2));
  d.p2.y = static_cast<lv_value_precise_t>(std::lround(y2));
  d.color = theme::c(color);
  d.width = w;
  d.opa = opa;
  d.round_start = d.round_end = w > 1;
  d.dash_width = dash;
  d.dash_gap = gap;
  lv_draw_line(layer, &d);
}

void drawText(lv_layer_t* layer, const char* text, int32_t x, int32_t y, int32_t w, lv_text_align_t align, uint32_t color,
              const lv_font_t* font = &font_m12) {
  lv_draw_label_dsc_t d;
  lv_draw_label_dsc_init(&d);
  d.text = text;
  d.text_local = 1;
  d.font = font;
  d.color = theme::c(color);
  d.align = align;
  lv_area_t a;
  a.x1 = x;
  a.y1 = y;
  a.x2 = x + w - 1;
  a.y2 = y + lv_font_get_line_height(font);
  lv_draw_label(layer, &d, &a);
}

class EcoPage : public Page {
 public:
  EcoPage() : Page("Eco") {}

  void create(lv_obj_t* parent) override {
    font38 = lv_font_montserrat_38;
    font38.fallback = &font_m28;

    // Kurve zuerst (liegt hinten), gezeichnet im Zeichen-Ereignis
    chart_ = lv_obj_create(parent);
    lv_obj_remove_style_all(chart_);
    lv_obj_set_pos(chart_, 0, 0);
    lv_obj_set_size(chart_, BOARD_LCD_HOR_RES, CY0 + CH + 20);
    lv_obj_remove_flag(chart_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(chart_, drawCb, LV_EVENT_DRAW_MAIN, this);
    for (float& v : curve_.v) v = NAN;
    curve_.ref = curve_.goal = curve_.tank = NAN;

    lv_obj_t* l = theme::label(parent, &font_m12, true, "Momentanverbrauch");
    lv_obj_set_pos(l, PAD, 6);
    bigRow_ = lv_obj_create(parent);
    lv_obj_remove_style_all(bigRow_);
    lv_obj_set_pos(bigRow_, PAD, BIG_TOP);
    lv_obj_set_size(bigRow_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bigRow_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bigRow_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(bigRow_, UNIT_GAP, 0);
    lv_obj_remove_flag(bigRow_, LV_OBJ_FLAG_CLICKABLE);
    big_ = theme::label(bigRow_, &font38, false, fmt::NO_VALUE);
    unit_ = theme::label(bigRow_, &font_m12, true, "");
    lv_obj_set_style_pad_bottom(unit_, 6, 0);

    // Gang rechts oben
    lv_obj_t* gl = theme::label(parent, &font_m12, true, "Gang");
    lv_obj_align(gl, LV_ALIGN_TOP_RIGHT, -PAD, GEAR_TOP);
    gearRow_ = lv_obj_create(parent);
    lv_obj_remove_style_all(gearRow_);
    lv_obj_set_size(gearRow_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(gearRow_, LV_ALIGN_TOP_RIGHT, -PAD, GEAR_TOP + 14);
    lv_obj_set_flex_flow(gearRow_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gearRow_, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(gearRow_, 3, 0);
    lv_obj_remove_flag(gearRow_, LV_OBJ_FLAG_CLICKABLE);
    arrow_ = theme::label(gearRow_, &font_m20, false, SYM_UP);
    lv_obj_set_style_text_color(arrow_, theme::c(theme::GOOD), 0);
    lv_obj_add_flag(arrow_, LV_OBJ_FLAG_HIDDEN);
    gear_ = theme::label(gearRow_, &font_m28, false, fmt::NO_VALUE);

    // Hinweis-Feld
    hint_ = lv_obj_create(parent);
    lv_obj_remove_style_all(hint_);
    lv_obj_set_size(hint_, HINT_W, HINT_H);
    lv_obj_set_pos(hint_, BOARD_LCD_HOR_RES - HINT_RIGHT - HINT_W, HINT_TOP);
    lv_obj_set_style_bg_color(hint_, theme::c(theme::SURFACE), 0);
    lv_obj_set_style_bg_opa(hint_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hint_, 1, 0);
    lv_obj_set_style_radius(hint_, 8, 0);
    lv_obj_set_style_pad_left(hint_, 5, 0);
    lv_obj_set_style_pad_right(hint_, 5, 0);
    lv_obj_set_flex_flow(hint_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hint_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(hint_, 4, 0);
    lv_obj_remove_flag(hint_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(hint_, LV_OBJ_FLAG_SCROLLABLE);
    pill_ = lv_obj_create(hint_);
    lv_obj_remove_style_all(pill_);
    lv_obj_set_size(pill_, LV_SIZE_CONTENT, PILL_H);
    lv_obj_set_style_min_width(pill_, 14, 0);
    lv_obj_set_style_radius(pill_, 9, 0);
    lv_obj_set_style_bg_opa(pill_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(pill_, 3, 0);
    lv_obj_remove_flag(pill_, LV_OBJ_FLAG_CLICKABLE);
    pillText_ = theme::label(pill_, &font_m12, false, "");
    lv_obj_set_style_text_color(pillText_, theme::c(theme::BG), 0);
    lv_obj_center(pillText_);
    lv_obj_t* col = lv_obj_create(hint_);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_CLICKABLE);
    hintText_ = theme::label(col, &font_m12, false, "");
    hintText2_ = theme::label(col, &font_m12, false, "");
    lv_obj_add_flag(hint_, LV_OBJ_FLAG_HIDDEN);

    // Untere Leiste
    const int32_t w = (BOARD_LCD_HOR_RES - 2 * BAR_SIDE - 2 * BAR_GAP) / 3;
    static const char* const LABELS[3] = {"Eco-Score", "Schub gespart", "Gebremst"};
    for (int i = 0; i < 3; i++) {
      lv_obj_t* t = lv_obj_create(parent);
      lv_obj_remove_style_all(t);
      lv_obj_set_size(t, w, BAR_H);
      lv_obj_set_pos(t, BAR_SIDE + i * (w + BAR_GAP), theme::CONTENT_H - BAR_BOTTOM - BAR_H);
      lv_obj_set_style_bg_color(t, theme::c(theme::SURFACE), 0);
      lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
      lv_obj_set_style_radius(t, theme::RADIUS_TILE, 0);
      lv_obj_set_style_pad_hor(t, BAR_PAD_HOR, 0);
      lv_obj_set_style_pad_ver(t, BAR_PAD_VER, 0);
      lv_obj_set_flex_flow(t, LV_FLEX_FLOW_COLUMN);
      lv_obj_remove_flag(t, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_remove_flag(t, LV_OBJ_FLAG_SCROLLABLE);
      theme::label(t, &font_m12, true, LABELS[i]);
      bar_[i] = theme::label(t, &font_m14, false, fmt::NO_VALUE);
    }
    lv_obj_set_style_text_color(bar_[1], theme::c(theme::GOOD), 0);
  }

  // Spartipps und Schaltpfeil laufen immer mit, auch wenn die Seite nicht sichtbar ist
  void tick(const CarSnapshot& s) override {
    eco::TipInput in;
    in.nowMs = s.now;
    in.engineOn = s.engineRunning();
    in.speedKmh = s.speed.get(s.now);
    in.accelMs2 = s.accel.get(s.now);
    in.pedalPct = s.pedalUsed.get(s.now);
    in.fuelCut = s.fuelCut;
    in.gear = s.gear;
    in.coolantC = s.coolant.get(s.now);
    in.rpm = s.rpm.get(s.now);
    in.coldCoolantC = s.profile.coldCoolantC;
    in.coldRpmLimit = s.profile.coldRpmLimit;
    in.tipsEnabled = uiprefs::get().tips != 0;  // Menü "Spartipps an/aus"
    // Tempo-Tipp: Ersparnis 100 statt 120 aus der eigenen Spartempo-Statistik (beide Klassen ≥ 5 km)
    constexpr int C100 = static_cast<int>((100 - cfg::TEMPO_FIRST_KMH) / cfg::TEMPO_STEP_KMH);
    constexpr int C120 = static_cast<int>((120 - cfg::TEMPO_FIRST_KMH) / cfg::TEMPO_STEP_KMH);
    if (s.tempoKm[C100] >= cfg::TEMPO_MIN_KM && s.tempoKm[C120] >= cfg::TEMPO_MIN_KM)
      in.tempoSaveL = s.tempoL[C120] / s.tempoKm[C120] * 100.0f - s.tempoL[C100] / s.tempoKm[C100] * 100.0f;
    tempoSave_ = in.tempoSaveL;
    in.imuLongMs2 = s.imuReady ? s.imuLong.get(s.now) : NAN;
    in.slopePct = s.slopePct.get(s.now);
    in.thermoSeq = s.thermoSeq;
    in.quiet = overlay::isOpen() || s.now - touch::lastTouchMs() < cfg::TIP_TOUCH_QUIET_MS;
    const eco::Tip before = tip_;
    tip_ = tips_.update(in);
    if (tip_ != before) {
      static const char* const NAMES[] = {"aus", "Gang rein", "Sanfter Gas", "Früher vom Gas", "Langer Stand", "Gleichmäßig",
                                          "Tempo", "Motor kalt", "Thermostat"};
      Serial.printf("Hinweis: %s\n", NAMES[static_cast<int>(tip_)]);
    }
    // Pfeil, sobald die Schaltempfehlung 1 s anliegt (A9)
    if (s.shiftAdvice) {
      if (!shiftSince_) shiftSince_ = s.now ? s.now : 1;
    } else {
      shiftSince_ = 0;
    }
  }

  void update(const CarSnapshot& s) override {
    const bool engineOn = s.engineRunning();
    const float inst = s.fuelL100.get(s.now);
    const float lph = s.fuelLph.get(s.now);
    const float goal = s.goalL100.get(s.now);
    const float tank = s.avgTank.get(s.now);
    const float ref = std::isnan(goal) ? tank : goal;

    // Momentanverbrauch: Schub, Stand (l/h) oder l/100 km
    char text[24];
    const char* unit = "l/100 km";
    const lv_font_t* font = &font38;
    uint32_t color = theme::TEXT;
    if (!engineOn) {
      snprintf(text, sizeof(text), "%s", fmt::NO_VALUE);
      unit = std::isnan(s.rpm.get(s.now)) ? "" : "Motor aus";
    } else if (s.fuelCut) {
      snprintf(text, sizeof(text), "SCHUB 0,0");
      unit = "";
      font = &font_m28;
      color = theme::GOOD;
    } else if (std::isnan(inst)) {
      fmt::number(text, sizeof(text), lph, 1);
      unit = std::isnan(lph) ? "" : "l/h " SYM_DOT " Stand";
    } else {
      fmt::number(text, sizeof(text), inst, 1);
      color = refColor(inst, ref);
    }
    setLabel(big_, shownBig_, sizeof(shownBig_), text);
    if (font != shownFont_) {
      shownFont_ = font;
      lv_obj_set_style_text_font(big_, font, 0);
    }
    if (color != shownColor_) {
      shownColor_ = color;
      lv_obj_set_style_text_color(big_, theme::c(color), 0);
    }
    if (unit != shownUnit_) {
      shownUnit_ = unit;
      lv_label_set_text_static(unit_, unit);
    }

    // Gang
    char g[8];
    if (s.gear < 0)
      snprintf(g, sizeof(g), "%s", fmt::NO_VALUE);
    else if (s.gear == 0)
      snprintf(g, sizeof(g), "N");
    else
      snprintf(g, sizeof(g), "%d", s.gear);
    setLabel(gear_, shownGear_, sizeof(shownGear_), g);
    const bool arrow = shiftSince_ && s.now - shiftSince_ >= cfg::SHIFT_ARROW_DELAY_MS && !s.fuelCut;
    if (arrow != shownArrow_) {
      shownArrow_ = arrow;
      if (arrow) lv_obj_remove_flag(arrow_, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(arrow_, LV_OBJ_FLAG_HIDDEN);
    }

    updateHint(s);

    // Untere Leiste
    char v[24];
    const float score = s.ecoScore.get(s.now);
    fmt::number(v, sizeof(v), score, 0);
    setLabel(bar_[0], shownBar_[0], sizeof(shownBar_[0]), v);
    const uint32_t sc = scoreColor(score);
    if (sc != shownScoreColor_) {
      shownScoreColor_ = sc;
      lv_obj_set_style_text_color(bar_[0], theme::c(sc), 0);
    }
    litersText(v, sizeof(v), s.cutSavedL.get(s.now));
    setLabel(bar_[1], shownBar_[1], sizeof(shownBar_[1]), v);
    litersText(v, sizeof(v), s.brakedL.get(s.now));
    setLabel(bar_[2], shownBar_[2], sizeof(shownBar_[2]), v);

    // Kurve: Tank, 100 km, 10 km, 1 km, Momentan (Schub = 0, Stand = kein Punkt)
    Curve c;
    c.v[0] = tank;
    c.v[1] = s.avg100.get(s.now);
    c.v[2] = s.avg10.get(s.now);
    c.v[3] = s.avg1.get(s.now);
    c.v[4] = !engineOn ? NAN : (s.fuelCut ? 0.0f : inst);
    c.ref = ref;
    c.goal = goal;
    c.tank = tank;
    c.tripV = s.avgTrip.get(s.now);
    c.tripPos = std::isnan(c.tripV) ? NAN : eco::tripAxisPos(s.tripKm.get(s.now), s.fillKm.get(s.now));
    if (!sameCurve(c, curve_) && s.now - lastDraw_ >= cfg::CHART_MIN_REDRAW_MS) {
      curve_ = c;
      lastDraw_ = s.now;
      lv_obj_invalidate(chart_);
    }
  }

  void onShow() override { lv_obj_invalidate(chart_); }

 private:
  static void litersText(char* out, size_t size, float l) {
    char n[16];
    fmt::number(n, sizeof(n), l, 2);
    if (std::isnan(l))
      snprintf(out, size, "%s", n);
    else
      snprintf(out, size, "%s l", n);
  }

  static void setLabel(lv_obj_t* l, char* shown, size_t size, const char* text) {
    if (strcmp(shown, text) == 0) return;
    snprintf(shown, size, "%s", text);
    lv_label_set_text(l, text);
  }

  void updateHint(const CarSnapshot& s) {
    char pill[16] = "", txt[32] = "", txt2[32] = "";
    bool warn = false;
    const lv_font_t* font2 = &font_m12;
    switch (tip_) {
      case eco::Tip::Coast:
        snprintf(pill, sizeof(pill), "0 l");
        snprintf(txt, sizeof(txt), "Gang rein");
        snprintf(txt2, sizeof(txt2), "Schub 0 l");
        break;
      case eco::Tip::HardPedal:
        snprintf(pill, sizeof(pill), SYM_DOWN);
        snprintf(txt, sizeof(txt), "Sanfter");
        snprintf(txt2, sizeof(txt2), "Gas geben");
        break;
      case eco::Tip::LateLift:
        snprintf(pill, sizeof(pill), "\xE2\x86\x92");  // →
        snprintf(txt, sizeof(txt), "Früher");
        snprintf(txt2, sizeof(txt2), "vom Gas");
        break;
      case eco::Tip::Idle: {
        const uint32_t sec = tips_.standMs(s.now) / 1000;
        snprintf(pill, sizeof(pill), "P");
        snprintf(txt, sizeof(txt), "Stand %u:%02u", (unsigned)(sec / 60), (unsigned)(sec % 60));
        snprintf(txt2, sizeof(txt2), "Motor aus?");
        break;
      }
      case eco::Tip::Tempo: {
        char n[12];
        fmt::number(n, sizeof(n), tempoSave_, 1);
        snprintf(pill, sizeof(pill), "%s", "100");
        snprintf(txt, sizeof(txt), "100 statt 120");
        snprintf(txt2, sizeof(txt2), "\xE2\x80\x93%s l/100 km", n);
        font2 = &font_small;
        break;
      }
      case eco::Tip::Thermo:
        snprintf(pill, sizeof(pill), "!");
        snprintf(txt, sizeof(txt), "Motor bleibt kalt");
        snprintf(txt2, sizeof(txt2), "Thermostat?");
        warn = true;
        break;
      case eco::Tip::Steady:
        snprintf(pill, sizeof(pill), "\xE2\x89\x88");  // ≈
        snprintf(txt, sizeof(txt), "Gleichmäßig");
        snprintf(txt2, sizeof(txt2), "Gas halten");
        break;
      case eco::Tip::Cold: {
        char t[12], r[16];
        fmt::number(t, sizeof(t), s.coolant.get(s.now), 0);
        fmt::number(r, sizeof(r), s.profile.coldRpmLimit, 0);
        snprintf(pill, sizeof(pill), SYM_THERMO);
        snprintf(txt, sizeof(txt), "Motor %s " SYM_DEG "C", t);
        snprintf(txt2, sizeof(txt2), "max. %s U/min", r);  // kleiner, wie in der Vorschau
        font2 = &lv_font_montserrat_10;
        warn = true;
        break;
      }
      default:
        break;
    }
    const bool vis = pill[0] != '\0';
    if (vis != shownHint_) {
      shownHint_ = vis;
      if (vis) lv_obj_remove_flag(hint_, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(hint_, LV_OBJ_FLAG_HIDDEN);
    }
    if (!vis) return;
    const uint32_t c = warn ? theme::WARN : theme::ACCENT;
    if (c != shownHintColor_) {
      shownHintColor_ = c;
      lv_obj_set_style_border_color(hint_, theme::c(c), 0);
      lv_obj_set_style_bg_color(pill_, theme::c(c), 0);
    }
    setLabel(pillText_, shownPill_, sizeof(shownPill_), pill);
    setLabel(hintText_, shownHintText_, sizeof(shownHintText_), txt);
    if (font2 != shownFont2_) {
      shownFont2_ = font2;
      lv_obj_set_style_text_font(hintText2_, font2, 0);
    }
    setLabel(hintText2_, shownHintText2_, sizeof(shownHintText2_), txt2);
  }

  static void drawCb(lv_event_t* e) {
    static_cast<EcoPage*>(lv_event_get_user_data(e))->draw(lv_event_get_layer(e), lv_event_get_target_obj(e));
  }

  void draw(lv_layer_t* layer, lv_obj_t* obj) {
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    const int32_t ox = a.x1, oy = a.y1;
    const Curve& c = curve_;
    char t[16];

    // Raster mit Beschriftung links
    for (int v = 0; v <= static_cast<int>(cfg::ECO_CURVE_TOP_L100); v += GRID_STEP) {
      const float y = oy + sy(static_cast<float>(v));
      drawLine(layer, ox + CX0, y, ox + CX1, y, theme::LINE, 1);
      snprintf(t, sizeof(t), "%d", v);
      drawText(layer, t, ox, static_cast<int32_t>(y) - 7, CX0 - 5, LV_TEXT_ALIGN_RIGHT, theme::MUTED);
    }
    // Bezugslinie: Spar-Ziel grün gestrichelt mit Beschriftung, sonst Tank-Schnitt in accent
    if (!std::isnan(c.goal)) {
      const float y = oy + sy(c.goal);
      drawLine(layer, ox + CX0, y, ox + CX1, y, theme::GOOD, 1, LV_OPA_COVER, 4, 3);
      char n[12];
      fmt::number(n, sizeof(n), c.goal, 1);
      snprintf(t, sizeof(t), "Ziel %s", n);
      drawText(layer, t, ox + CX0 + 4, static_cast<int32_t>(y) + 1, 80, LV_TEXT_ALIGN_LEFT, theme::GOOD);
    } else if (!std::isnan(c.tank)) {
      const float y = oy + sy(c.tank);
      drawLine(layer, ox + CX0, y, ox + CX1, y, theme::ACCENT, 1, REF_OPA, 3, 3);
    }

    // Punkte mit Wert
    float px[POINTS], py[POINTS];
    int n = 0;
    for (int i = 0; i < POINTS; i++) {
      if (std::isnan(c.v[i])) continue;
      px[n] = ox + sx(i);
      py[n] = oy + sy(c.v[i]);
      n++;
    }
    // Runde Kurve (Catmull-Rom als Bezier) mit Flächenfüllung
    if (n >= 2) {
      float prevX = px[0], prevY = py[0];
      for (int i = 0; i < n - 1; i++) {
        const float x0 = px[i > 0 ? i - 1 : i], y0 = py[i > 0 ? i - 1 : i];
        const float x1 = px[i], y1 = py[i], x2 = px[i + 1], y2 = py[i + 1];
        const float x3 = px[i + 2 < n ? i + 2 : i + 1], y3 = py[i + 2 < n ? i + 2 : i + 1];
        const float c1x = x1 + (x2 - x0) / 6, c1y = y1 + (y2 - y0) / 6;
        const float c2x = x2 - (x3 - x1) / 6, c2y = y2 - (y3 - y1) / 6;
        for (int k = 1; k <= SAMPLES_PER_SEG; k++) {
          const float u = static_cast<float>(k) / SAMPLES_PER_SEG, m = 1 - u;
          const float x = m * m * m * x1 + 3 * m * m * u * c1x + 3 * m * u * u * c2x + u * u * u * x2;
          const float y = m * m * m * y1 + 3 * m * m * u * c1y + 3 * m * u * u * c2y + u * u * u * y2;
          // Fläche: senkrechte Striche bis zur Grundlinie, je Pixelspalte einer
          for (int xi = static_cast<int>(std::ceil(prevX)); xi < static_cast<int>(std::ceil(x)); xi++) {
            const float f = (xi - prevX) / (x - prevX > 0.01f ? x - prevX : 0.01f);
            const float yy = prevY + f * (y - prevY);
            drawLine(layer, xi, yy + 1, xi, oy + CY0 + CH, theme::ACCENT, 1, FILL_OPA);
          }
          drawLine(layer, prevX, prevY, x, y, theme::ACCENT, CURVE_W);
          prevX = x;
          prevY = y;
        }
      }
    }
    const bool trip = !std::isnan(c.tripPos);
    const float tx = trip ? ox + sx(c.tripPos) : 0;
    for (int i = 0; i < POINTS; i++) {
      const float x = ox + sx(i);
      if (!trip || std::fabs(x - tx) >= TRIP_LABEL_GAP)
        drawText(layer, X_LABELS[i], static_cast<int32_t>(x) - 36, oy + CY0 + CH + 3, 72, LV_TEXT_ALIGN_CENTER, theme::MUTED);
      if (std::isnan(c.v[i])) {
        if (i == POINTS - 1)
          drawText(layer, "Stand", static_cast<int32_t>(x) - 30, oy + CY0 + CH - 18, 60, LV_TEXT_ALIGN_CENTER, theme::MUTED);
        continue;
      }
      const uint32_t col = (i == 0 && std::isnan(c.goal)) ? theme::ACCENT : refColor(c.v[i], c.ref);
      const float y = oy + sy(c.v[i]);
      const int32_t r = i == POINTS - 1 ? DOT_R_NOW : DOT_R;
      lv_draw_rect_dsc_t d;
      lv_draw_rect_dsc_init(&d);
      d.radius = LV_RADIUS_CIRCLE;
      d.bg_color = theme::c(col);
      d.bg_opa = LV_OPA_COVER;
      d.border_color = theme::c(theme::BG);
      d.border_width = 2;
      d.border_opa = LV_OPA_COVER;
      lv_area_t da;
      da.x1 = static_cast<int32_t>(std::lround(x)) - r;
      da.y1 = static_cast<int32_t>(std::lround(y)) - r;
      da.x2 = da.x1 + 2 * r;
      da.y2 = da.y1 + 2 * r;
      lv_draw_rect(layer, &d, &da);
      // Wert über dem Punkt, wenn oben Platz ist, sonst darunter; über 12 mit ↑
      char n2[16];
      fmt::number(n2, sizeof(n2), c.v[i], 1);
      snprintf(t, sizeof(t), "%s%s", n2, c.v[i] > cfg::ECO_CURVE_TOP_L100 ? "\xE2\x86\x91" : "");
      const bool above = y - 7 > oy + CY0 - 4;
      drawText(layer, t, static_cast<int32_t>(x) - 30, static_cast<int32_t>(above ? y - 20 : y + 6), 60, LV_TEXT_ALIGN_CENTER, col);
    }
    if (trip) drawTrip(layer, ox, oy, tx, c);
  }

  // Fahrt-Punkt (Jos Wunsch): Ring auf Höhe Ø Fahrt, wandert mit den gefahrenen km über die Achse;
  // gestrichelte Linie zur Achse, darunter "Fahrt", Wert seitlich neben dem Ring (feste Werte stehen darüber/darunter)
  void drawTrip(lv_layer_t* layer, int32_t ox, int32_t oy, float x, const Curve& c) {
    const uint32_t col = refColor(c.tripV, c.ref);
    const float y = oy + sy(c.tripV);
    drawLine(layer, x, y + TRIP_R, x, oy + CY0 + CH, theme::MUTED, 1, LV_OPA_COVER, 2, 2);
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.radius = LV_RADIUS_CIRCLE;
    d.bg_color = theme::c(theme::BG);
    d.bg_opa = LV_OPA_COVER;
    d.border_color = theme::c(col);
    d.border_width = 2;
    d.border_opa = LV_OPA_COVER;
    lv_area_t da;
    da.x1 = static_cast<int32_t>(std::lround(x)) - TRIP_R;
    da.y1 = static_cast<int32_t>(std::lround(y)) - TRIP_R;
    da.x2 = da.x1 + 2 * TRIP_R;
    da.y2 = da.y1 + 2 * TRIP_R;
    lv_draw_rect(layer, &d, &da);
    drawText(layer, "Fahrt", static_cast<int32_t>(x) - 30, oy + CY0 + CH + 3, 60, LV_TEXT_ALIGN_CENTER, col);
    char n[16], t[20];
    fmt::number(n, sizeof(n), c.tripV, 1);
    snprintf(t, sizeof(t), "%s%s", n, c.tripV > cfg::ECO_CURVE_TOP_L100 ? "\xE2\x86\x91" : "");
    const bool right = x < ox + sx(3.5f);  // rechts daneben, außer kurz vor "Momentan"
    const int32_t yt = static_cast<int32_t>(y) - 7;
    if (right)
      drawText(layer, t, static_cast<int32_t>(x) + TRIP_R + 3, yt, 40, LV_TEXT_ALIGN_LEFT, col);
    else
      drawText(layer, t, static_cast<int32_t>(x) - TRIP_R - 43, yt, 40, LV_TEXT_ALIGN_RIGHT, col);
  }

  lv_obj_t* chart_ = nullptr;
  lv_obj_t* bigRow_ = nullptr;
  lv_obj_t* big_ = nullptr;
  lv_obj_t* unit_ = nullptr;
  lv_obj_t* gearRow_ = nullptr;
  lv_obj_t* gear_ = nullptr;
  lv_obj_t* arrow_ = nullptr;
  lv_obj_t* hint_ = nullptr;
  lv_obj_t* pill_ = nullptr;
  lv_obj_t* pillText_ = nullptr;
  lv_obj_t* hintText_ = nullptr;
  lv_obj_t* hintText2_ = nullptr;
  const lv_font_t* shownFont2_ = &font_m12;
  char shownHintText2_[32] = "";
  lv_obj_t* bar_[3] = {};

  char shownBig_[24] = "";
  const lv_font_t* shownFont_ = nullptr;
  uint32_t shownColor_ = 0xFFFFFFFF;
  const char* shownUnit_ = nullptr;
  char shownGear_[8] = "";
  bool shownArrow_ = false;
  bool shownHint_ = false;
  uint32_t shownHintColor_ = 0;
  char shownPill_[16] = "";
  char shownHintText_[32] = "";
  char shownBar_[3][24] = {};
  uint32_t shownScoreColor_ = 0xFFFFFFFF;

  Curve curve_;
  uint32_t lastDraw_ = 0;
  eco::TipEngine tips_;
  eco::Tip tip_ = eco::Tip::None;
  uint32_t shiftSince_ = 0;
  float tempoSave_ = NAN;
};

}  // namespace

Page* ecoPage() {
  static EcoPage page;
  return &page;
}
