// Seite "Fahrt & Tank" (U Seite 6): zwei Spalten Fahrt und Tankfüllung, Reichweite mit geteiltem Balken,
// Knopf "Getankt" öffnet das Tank-Fenster. Positionen aus der Vorschau (pFahrt).
#include <cmath>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "page.h"
#include "ui/range_bar.h"
#include "ui/symbols.h"
#include "ui/tank_dialog.h"
#include "ui/theme.h"
#include "ui/values.h"
#include "util/format.h"

namespace {

constexpr int32_t COL1_X = 10, COL2_X = 168, COL_W = 146, COL_TOP = 6;
constexpr int32_t ROW_H = 19;            // .kv: line-height 19 px
constexpr int ROWS = 6;
constexpr int32_t DIVIDER_X = 160, DIVIDER_TOP = 10, DIVIDER_H = 150;
constexpr int32_t BAR_Y = 146, BAR_W = 142;
constexpr int32_t BTN_Y = 176, BTN_W = 142, BTN_H = 34;
constexpr int32_t NOTE_Y = 180, NOTE_W = 150;

struct Column {
  lv_obj_t* key[ROWS] = {};
  lv_obj_t* val[ROWS] = {};
  char shown[ROWS][24] = {};
  uint32_t color[ROWS] = {};
};

void hmm(char* out, size_t size, float s) {
  if (std::isnan(s)) {
    snprintf(out, size, "%s", fmt::NO_VALUE);
    return;
  }
  const unsigned t = static_cast<unsigned>(s);
  snprintf(out, size, "%u:%02u h", t / 3600, t / 60 % 60);
}

void mmss(char* out, size_t size, float s) {
  if (std::isnan(s)) {
    snprintf(out, size, "%s", fmt::NO_VALUE);
    return;
  }
  const unsigned t = static_cast<unsigned>(s);
  snprintf(out, size, "%u:%02u min", t / 60, t % 60);
}

void withUnit(char* out, size_t size, float v, int dec, const char* unit) {
  char n[16];
  fmt::number(n, sizeof(n), v, dec);
  if (std::isnan(v))
    snprintf(out, size, "%s", n);
  else
    snprintf(out, size, "%s %s", n, unit);
}

class TripTankPage : public Page {
 public:
  TripTankPage() : Page("Fahrt & Tank") {}

  void create(lv_obj_t* parent) override {
    static const char* const TRIP_KEYS[ROWS] = {"Strecke", "Dauer", SYM_AVG " Verbrauch", "Kosten", "Leerlauf", "Eco-Score"};
    static const char* const TANK_KEYS[ROWS] = {"Gefahren", "Verbraucht", "Im Tank", "Reichweite", SYM_AVG "-Preis im Tank",
                                               "Kosten/100 km"};
    tripTitle_ = makeColumn(parent, COL1_X, "Fahrt", TRIP_KEYS, trip_);
    lastHint_ = theme::label(parent, &font_m12, true, "letzte Fahrt");
    lv_obj_set_pos(lastHint_, COL1_X + 44, COL_TOP + 2);
    lv_obj_add_flag(lastHint_, LV_OBJ_FLAG_HIDDEN);
    makeColumn(parent, COL2_X, "Tankfüllung", TANK_KEYS, tank_);
    lv_obj_t* div = lv_obj_create(parent);
    lv_obj_remove_style_all(div);
    lv_obj_set_pos(div, DIVIDER_X, DIVIDER_TOP);
    lv_obj_set_size(div, 1, DIVIDER_H);
    lv_obj_set_style_bg_color(div, theme::c(theme::LINE), 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    bar_.create(parent, BAR_W);
    lv_obj_set_pos(bar_.obj(), COL2_X, BAR_Y);

    lv_obj_t* b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_pos(b, COL2_X, BTN_Y);
    lv_obj_set_size(b, BTN_W, BTN_H);
    lv_obj_set_style_radius(b, theme::RADIUS_TILE, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, theme::c(theme::LINE), 0);
    lv_obj_set_style_bg_color(b, theme::c(theme::SURFACE), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, theme::c(theme::LINE), LV_STATE_PRESSED);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b, onRefuel, LV_EVENT_SHORT_CLICKED, this);
    lv_obj_t* l = theme::label(b, &font_m14, false, "Getankt");
    lv_obj_center(l);

    note_ = theme::label(parent, &font_m12, true, "");
    lv_obj_set_pos(note_, COL1_X, NOTE_Y);
    lv_obj_set_width(note_, NOTE_W);
    lv_label_set_long_mode(note_, LV_LABEL_LONG_WRAP);
  }

  void update(const CarSnapshot& s) override {
    last_ = s;
    const uint32_t n = s.now;
    char t[24];
    // Fahrt; nach dem Fahrtende die Werte der letzten Fahrt, bis die neue 0,1 km hat (Jos Wunsch, A5)
    const float liveKm = s.tripKm.get(n);
    const bool showLast = s.hasLastTrip && (std::isnan(liveKm) || liveKm < cfg::TRIP_MIN_RECORD_KM);
    if (showLast != shownLast_) {
      shownLast_ = showLast;
      if (showLast) lv_obj_remove_flag(lastHint_, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(lastHint_, LV_OBJ_FLAG_HIDDEN);
    }
    const trip::TripRecord& lt = s.lastTrip;
    const float km = showLast ? lt.km : liveKm;
    const float l = showLast ? lt.liters : s.tripL.get(n);
    withUnit(t, sizeof(t), km, 1, "km");
    set(trip_, 0, t);
    hmm(t, sizeof(t), showLast ? lt.durationS : s.tripDurationS.get(n));
    set(trip_, 1, t);
    withUnit(t, sizeof(t), (!std::isnan(km) && km >= cfg::TRIP_AVG_MIN_KM) ? l / km * 100.0f : NAN, 1, "l/100");
    set(trip_, 2, t);
    withUnit(t, sizeof(t), showLast ? lt.cost : s.tripCost.get(n), 2, SYM_EURO);
    set(trip_, 3, t);
    mmss(t, sizeof(t), showLast ? lt.idleS : s.tripIdleS.get(n));
    set(trip_, 4, t);
    const float score = showLast ? lt.ecoScore : s.ecoScore.get(n);
    fmt::number(t, sizeof(t), score, 0);
    set(trip_, 5, t,
        std::isnan(score) ? theme::TEXT : score >= cfg::SCORE_GOOD ? theme::GOOD : score < cfg::SCORE_OK ? theme::WARN : theme::TEXT);

    // Tankfüllung
    const float fkm = s.fillKm.get(n), fl = s.fillL.get(n);
    withUnit(t, sizeof(t), fkm, 0, "km");
    set(tank_, 0, t);
    withUnit(t, sizeof(t), fl, 1, "l");
    set(tank_, 1, t);
    const float tankL = s.tankL.get(n);
    char num[16];
    fmt::number(num, sizeof(num), tankL, 1);
    if (std::isnan(tankL))
      snprintf(t, sizeof(t), "%s", num);
    else
      snprintf(t, sizeof(t), "\xE2\x89\x88 %s l", num);  // ≈
    set(tank_, 2, t);
    withUnit(t, sizeof(t), s.rangeKm.get(n), 0, "km");
    set(tank_, 3, t, values::color(values::Key::Range, s));
    withUnit(t, sizeof(t), s.mixPrice.get(n), 3, SYM_EURO "/l");
    set(tank_, 4, t);
    // Mit Spar-Ziel "Ø / Ziel", sonst Kosten je 100 km
    const float goal = s.goalL100.get(n);
    const float avg = (!std::isnan(fkm) && fkm >= cfg::TANK_GOAL_MIN_KM) ? fl / fkm * 100.0f : NAN;
    if (!std::isnan(goal)) {
      setKey(tank_, 5, SYM_AVG " / Ziel");
      char a[12], g[12];
      fmt::number(a, sizeof(a), avg, 1);
      fmt::number(g, sizeof(g), goal, 1);
      snprintf(t, sizeof(t), "%s / %s", a, g);
      set(tank_, 5, t, std::isnan(avg) ? theme::TEXT : (avg <= goal ? theme::GOOD : theme::WARN));
    } else {
      setKey(tank_, 5, "Kosten/100 km");
      const float mix = s.mixPrice.get(n);
      withUnit(t, sizeof(t), (!std::isnan(fkm) && fkm >= 1.0f && !std::isnan(mix)) ? fl * mix / fkm * 100.0f : NAN, 2, SYM_EURO);
      set(tank_, 5, t);
    }
    bar_.update(s);

    const char* note = (s.link.supportedKnown && !s.link.pidSupported(0x2F))
                           ? "Ohne Füllstandssensor: Tank ab letzter Vollbetankung berechnet"
                           : "";
    if (note != shownNote_) {
      shownNote_ = note;
      lv_label_set_text_static(note_, note);
    }
  }

 private:
  static void onRefuel(lv_event_t* e) {
    auto* p = static_cast<TripTankPage*>(lv_event_get_user_data(e));
    tankdlg::openManual(p->last_);
  }

  lv_obj_t* makeColumn(lv_obj_t* parent, int32_t x, const char* title, const char* const keys[ROWS], Column& c) {
    lv_obj_t* t = theme::label(parent, &font_m14, false, title);
    lv_obj_set_pos(t, x, COL_TOP);
    for (int i = 0; i < ROWS; i++) {
      const int32_t y = COL_TOP + 22 + i * ROW_H;
      c.key[i] = theme::label(parent, &font_m12, true, keys[i]);
      lv_obj_set_pos(c.key[i], x, y);
      c.val[i] = theme::label(parent, &font_m12, false, fmt::NO_VALUE);
      lv_obj_set_width(c.val[i], COL_W);
      lv_obj_set_style_text_align(c.val[i], LV_TEXT_ALIGN_RIGHT, 0);
      lv_obj_set_pos(c.val[i], x, y);
      c.color[i] = theme::TEXT;
    }
    return t;
  }

  static void set(Column& c, int i, const char* text, uint32_t color = theme::TEXT) {
    if (strcmp(c.shown[i], text) != 0) {
      snprintf(c.shown[i], sizeof(c.shown[i]), "%s", text);
      lv_label_set_text(c.val[i], text);
    }
    if (color != c.color[i]) {
      c.color[i] = color;
      lv_obj_set_style_text_color(c.val[i], theme::c(color), 0);
    }
  }

  static void setKey(Column& c, int i, const char* key) {
    if (strcmp(lv_label_get_text(c.key[i]), key) != 0) lv_label_set_text(c.key[i], key);
  }

  Column trip_, tank_;
  lv_obj_t* tripTitle_ = nullptr;
  lv_obj_t* lastHint_ = nullptr;
  bool shownLast_ = false;
  RangeBar bar_;
  lv_obj_t* note_ = nullptr;
  const char* shownNote_ = nullptr;
  CarSnapshot last_;
};

}  // namespace

Page* tripTankPage() {
  static TripTankPage page;
  return &page;
}
