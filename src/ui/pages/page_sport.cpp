// Seite "Sport" (A10, U Seite 2), umgebaut am 7.10.2026 nach der ersten Fahrt (Jos Wunsch):
// großer Drehzahlbogen 240° (0–6500 U/min, ab 5800 warn) mit großem Tempo und Gang, rechts die vier
// Live-Balken (Leistung, Beschleunigung, Gaspedal, Saugrohrdruck), darunter eine Wettbewerbszeile und unten
// vier frei belegbare Felder (Ø Tempo, Strecke, Fahrzeit, Leistung). Das Live-Diagramm der letzten 30 s
// erscheint groß, wenn man auf den Bogen tippt.
//
// Wettbewerbszeile (in dieser Reihenfolge):
//  1. 8 s nach einem Sprint-Ergebnis: Zeit und Abstand zur Bestzeit ("Bestzeit!" grün, sonst "+0,4 s" orange)
//  2. beim kräftigen Beschleunigen: Prozent der besten Beschleunigung dieser Fahrt ("Rekord!" ab 100 %)
//  3. sonst "Zeit gewonnen": Wie viel Zeit die letzten 2 min gegenüber dem bisherigen Fahrtschnitt gebracht
//     haben, dazu die Änderung des Ø Tempos
#include <cmath>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "page.h"
#include "ui/field_bar.h"
#include "ui/live.h"
#include "ui/overlay.h"
#include "ui/symbols.h"
#include "ui/theme.h"
#include "ui/ui_prefs.h"
#include "ui/values.h"
#include "util/format.h"

namespace {

using live::Ser;

// Drehzahlbogen links, groß
constexpr int32_t CX = 86, CY = 86, R = 74, ARC_W = 12;
constexpr int32_t A0 = 150, SWEEP = 240;
// Balken rechts
constexpr int32_t BAR_X = 178, BAR_RIGHT = 10;
constexpr int32_t BAR_TOPS[4] = {6, 36, 66, 96};
constexpr int32_t BAR_H = 6;
// Wettbewerbszeile und Felder
constexpr int32_t LINE_Y = 132;
constexpr int32_t FIELDS_H = 48, FIELDS_BOTTOM = 5;
// Live-Diagramm im Fenster
constexpr int32_t CH_X0 = 30, CH_X1 = 262, CH_Y0 = 10, CH_H = 130;
constexpr int DASH_PX = 3;
// Wettbewerb
constexpr uint32_t RESULT_SHOW_MS = 8000;
constexpr float ACC_SHOW_MS2 = 1.0f;        // Beschleunigungs-Urteil ab 1 m/s² und 5 km/h
constexpr uint32_t TREND_SAMPLE_MS = 10000; // Schnitt alle 10 s merken ...
constexpr int TREND_SAMPLES = 13;           // ... 13 Proben = 2 min zurück
constexpr float TREND_MIN_KMH = 0.5f;       // kleiner: neutral
constexpr float TREND_MIN_TRIP_S = 300.0f;  // erst nach 5 min Fahrt (vorher schwankt der Schnitt zu stark)

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
  lv_obj_t* mark = nullptr;  // Beschleunigung: bester Wert der Fahrt
  bool bipolar = false;
  char shown[32] = "";
  int32_t shownX = -1, shownW = -1, shownMark = -1;
  uint32_t shownColor = 0;
};

int pair() {
  const int p = uiprefs::get().sportPair;
  return p < 3 ? p : 0;
}

// Live-Diagramm (Fenster): Rahmen x0..x1, y0..y0+h
void drawLive(lv_layer_t* layer, int32_t x0, int32_t x1, int32_t y0, int32_t h) {
  for (int f = 0; f <= 2; f++) line(layer, x0, y0 + h * f / 2, x1, y0 + h * f / 2, theme::LINE, 1);
  const Ser* pr = PAIRS[pair()];
  if (pr[0] == Ser::Acc) {
    const int32_t yz = y0 + h - static_cast<int32_t>((0 - live::lo(Ser::Acc)) / (live::hi(Ser::Acc) - live::lo(Ser::Acc)) * h);
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
      const int32_t y = y0 + h - static_cast<int32_t>(std::lround(c * h));
      if (pen && (idx == 0 || ((x - x0) / DASH_PX) % 2 == 0)) line(layer, px, py, x, y, col, 2);
      px = x;
      py = y;
      pen = true;
    }
    char t[12];
    fmt::number(t, sizeof(t), live::hi(r), 0);
    const int32_t ax = idx ? x1 + 3 : x0 - 3 - 28;
    const lv_text_align_t al = idx ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_RIGHT;
    text(layer, t, ax, y0 - 6, 28, al, col, &font_m12);
    fmt::number(t, sizeof(t), live::lo(r), 0);
    text(layer, t, ax, y0 + h - 8, 28, al, col, &font_m12);
  }
}

class SportPage;
SportPage* self = nullptr;

class SportPage : public Page {
 public:
  SportPage() : Page("Sport") { self = this; }

  void create(lv_obj_t* parent) override {
    gauge_ = lv_obj_create(parent);
    lv_obj_remove_style_all(gauge_);
    lv_obj_set_pos(gauge_, 0, 0);
    lv_obj_set_size(gauge_, BAR_X - 4, LINE_Y);
    lv_obj_add_flag(gauge_, LV_OBJ_FLAG_CLICKABLE);  // Tippen: Live-Diagramm groß
    lv_obj_add_event_cb(gauge_, onGaugeTap, LV_EVENT_SHORT_CLICKED, this);
    lv_obj_add_event_cb(gauge_, onDraw, LV_EVENT_DRAW_MAIN, this);

    // Tempo groß und Gang in der Mitte des Bogens
    speed_ = theme::label(parent, &font_m48, false, fmt::NO_VALUE);
    lv_obj_set_width(speed_, 2 * R);
    lv_obj_set_style_text_align(speed_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(speed_, CX - R, CY - 36);
    lv_obj_t* u = theme::label(parent, &font_m12, true, "km/h");
    lv_obj_set_width(u, 2 * R);
    lv_obj_set_style_text_align(u, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(u, CX - R, CY + 14);
    gearRow_ = lv_obj_create(parent);
    lv_obj_remove_style_all(gearRow_);
    lv_obj_set_size(gearRow_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(gearRow_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gearRow_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(gearRow_, 4, 0);
    lv_obj_remove_flag(gearRow_, LV_OBJ_FLAG_CLICKABLE);
    arrow_ = theme::label(gearRow_, &font_m20, false, SYM_UP);
    lv_obj_set_style_text_color(arrow_, theme::c(theme::GOOD), 0);
    lv_obj_add_flag(arrow_, LV_OBJ_FLAG_HIDDEN);
    gear_ = theme::label(gearRow_, &font_v24, false, fmt::NO_VALUE);
    lv_obj_t* gl = theme::label(gearRow_, &font_m12, true, "Gang");
    lv_obj_set_style_pad_bottom(gl, 4, 0);
    lv_obj_align(gearRow_, LV_ALIGN_TOP_MID, CX - BOARD_LCD_HOR_RES / 2, CY + 30);

    static const char* const LABELS[4] = {"Leistung", "Beschl.", "Gaspedal", "Saugrohr"};
    for (int i = 0; i < 4; i++) makeBar(parent, i, LABELS[i], i == 1);

    // Wettbewerbszeile
    compete_ = theme::label(parent, &font_m20, false, "");
    lv_obj_set_width(compete_, BOARD_LCD_HOR_RES - 16);
    lv_obj_set_style_text_align(compete_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(compete_, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(compete_, 8, LINE_Y + 2);

    fields_.create(parent, 1, 4, theme::CONTENT_H - FIELDS_BOTTOM - FIELDS_H, FIELDS_H);
  }

  void onShow() override { lv_obj_invalidate(gauge_); }

  void tick(const CarSnapshot& s) override {
    if (s.shiftAdvice) {
      if (!shiftSince_) shiftSince_ = s.now ? s.now : 1;
    } else {
      shiftSince_ = 0;
    }
    // Schnitt-Trend: alle 10 s das Ø Tempo der Fahrt merken
    if (!trendAt_ || s.now - trendAt_ >= TREND_SAMPLE_MS) {
      trendAt_ = s.now ? s.now : 1;
      trendKm_[trendHead_] = s.tripKm.get(s.now);
      trendDur_[trendHead_] = s.tripDurationS.get(s.now);
      trendHead_ = (trendHead_ + 1) % TREND_SAMPLES;
      if (trendCount_ < TREND_SAMPLES) trendCount_++;
    }
    // Beste Beschleunigung dieser Sitzung (für das Urteil): zählt erst, wenn eine Beschleunigungsphase endet,
    // damit die laufende Phase mit dem bisherigen Rekord verglichen wird
    const float acc = s.imuReady ? s.imuLong.get(s.now) : s.accel.get(s.now);
    if (!std::isnan(acc) && acc >= ACC_SHOW_MS2) {
      if (std::isnan(phaseMax_) || acc > phaseMax_) phaseMax_ = acc;
    } else if (!std::isnan(phaseMax_)) {
      if (std::isnan(accBest_) || phaseMax_ > accBest_) accBest_ = phaseMax_;
      phaseMax_ = NAN;
    }
    // Neues Sprint-Ergebnis
    if (s.sprint.resultSeq != seenResult_) {
      if (seenInit_) resultAt_ = s.now ? s.now : 1;
      seenResult_ = s.sprint.resultSeq;
    }
    seenInit_ = true;
    // Live-Diagramm im Fenster aktuell halten (auch wenn die Seite gewechselt wurde)
    if (chart_ && overlay::isOpen() && overlay::generation() == chartGen_ && live::seq() != drawnSeq_ &&
        s.now - chartDraw_ >= cfg::CHART_MIN_REDRAW_MS) {
      drawnSeq_ = live::seq();
      chartDraw_ = s.now;
      lv_obj_invalidate(chart_);
    }
  }

  void update(const CarSnapshot& s) override {
    last_ = s;
    char t[48];
    fmt::number(t, sizeof(t), values::value(values::Key::Speed, s), 0);
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
    char a[16];
    fmt::number(a, sizeof(a), kw, 0);
    snprintf(t, sizeof(t), std::isnan(kw) ? "%s" : "%s kW", a);
    setBar(0, t, std::isnan(kw) ? 0 : kw / (s.profile.powerKw > 0 ? s.profile.powerKw : 55), theme::ACCENT);
    // Mit MPU6050 gemessen und als Längs-G beschriftet (A10)
    const float accRaw = s.imuReady ? s.imuLong.get(s.now) : s.accel.get(s.now);
    if (s.imuReady != shownImu_) {
      shownImu_ = s.imuReady;
      lv_label_set_text_static(bars_[1].label, s.imuReady ? "Längs-G" : "Beschl.");
    }
    // auf 0,1 gerundet, damit kein "–0,0" erscheint
    const float acc = std::isnan(accRaw) ? NAN : std::round(accRaw * 10.0f) / 10.0f + 0.0f;
    fmt::number(a, sizeof(a), std::isnan(acc) ? NAN : std::fabs(acc), 1);
    if (std::isnan(acc))
      snprintf(t, sizeof(t), "%s", a);
    else
      snprintf(t, sizeof(t), "%s%s m/s" "\xC2\xB2", acc >= 0 ? "+" : "\xE2\x80\x93", a);  // – und ²
    setBar(1, t, std::isnan(acc) ? 0 : acc / 4.0f, (!std::isnan(acc) && acc < 0) ? theme::MUTED : theme::ACCENT);
    setMark(1, std::isnan(accBest_) || accBest_ < ACC_SHOW_MS2 ? NAN : accBest_ / 4.0f);
    const float pedal = values::value(values::Key::Pedal, s);
    fmt::number(a, sizeof(a), pedal, 0);
    snprintf(t, sizeof(t), std::isnan(pedal) ? "%s" : "%s %%", a);
    setBar(2, t, std::isnan(pedal) ? 0 : pedal / 100.0f, theme::ACCENT);
    const float map = s.map.get(s.now);
    fmt::number(a, sizeof(a), map, 0);
    snprintf(t, sizeof(t), std::isnan(map) ? "%s" : "%s kPa", a);
    setBar(3, t, std::isnan(map) ? 0 : map / 100.0f, theme::ACCENT);

    updateCompete(s, acc);
    fields_.update(s);

    // Bogen neu zeichnen, wenn sich die Drehzahl geändert hat (höchstens 5 Hz); Fenster mit dem Live-Diagramm
    const float rpm = s.rpm.get(s.now);
    const int rpmStep = std::isnan(rpm) ? -1 : static_cast<int>(rpm / 25);
    if (rpmStep != drawnRpm_ && s.now - lastDraw_ >= cfg::CHART_MIN_REDRAW_MS) {
      drawnRpm_ = rpmStep;
      lastDraw_ = s.now;
      lv_obj_invalidate(gauge_);
    }
  }

 private:
  void updateCompete(const CarSnapshot& s, float acc) {
    const SprintInfo& sp = s.sprint;
    char t[48] = "";
    uint32_t col = theme::MUTED;
    const float v = s.speed.get(s.now);
    if (resultAt_ && s.now - resultAt_ < RESULT_SHOW_MS && !std::isnan(sp.resultS)) {
      static const char* const K[] = {"", "0" "\xE2\x80\x93" "50", "0" "\xE2\x80\x93" "100", "80" "\xE2\x80\x93" "120"};
      char n[12];
      fmt::number(n, sizeof(n), sp.resultS, 1);
      const char* k = K[static_cast<int>(sp.resultKind) < 4 ? static_cast<int>(sp.resultKind) : 0];
      if (std::isnan(sp.resultPrevBest) || sp.resultS < sp.resultPrevBest) {
        snprintf(t, sizeof(t), "%s: %s s " SYM_DOT " Bestzeit!", k, n);
        col = theme::GOOD;
      } else {
        char d[12];
        fmt::number(d, sizeof(d), sp.resultS - sp.resultPrevBest, 1);
        snprintf(t, sizeof(t), "%s: %s s " SYM_DOT " +%s zur Best", k, n, d);
        col = theme::WARN;
      }
    } else if (!std::isnan(acc) && acc >= ACC_SHOW_MS2 && !std::isnan(v) && v > 5 && !std::isnan(accBest_) && accBest_ > 0) {
      const int pct = static_cast<int>(std::lround(acc / accBest_ * 100));
      if (pct > 100) {
        snprintf(t, sizeof(t), "Beschleunigung: Rekord!");
        col = theme::GOOD;
      } else {
        snprintf(t, sizeof(t), "Beschleunigung %d %% vom Rekord", pct);
        col = pct >= 85 ? theme::GOOD : theme::ACCENT;
      }
    } else if (trendCount_ >= TREND_SAMPLES) {
      // Zeit gewonnen: Strecke der letzten 2 min mit dem Fahrtschnitt von davor gefahren hätte so lange gedauert
      const int iNow = (trendHead_ + TREND_SAMPLES - 1) % TREND_SAMPLES, iThen = trendHead_;
      const float km1 = trendKm_[iNow], d1 = trendDur_[iNow], km0 = trendKm_[iThen], d0 = trendDur_[iThen];
      if (!std::isnan(km1) && !std::isnan(d1) && !std::isnan(km0) && !std::isnan(d0) && d0 > TREND_MIN_TRIP_S && km0 > 0.2f && d1 > d0) {
        const float avgOld = km0 / (d0 / 3600.0f), avgNow = km1 / (d1 / 3600.0f);
        const float gained = (km1 - km0) / avgOld * 3600.0f - (d1 - d0);
        const float dv = avgNow - avgOld;
        char n[12], m[12];
        fmt::number(n, sizeof(n), std::fabs(gained), 0);
        fmt::number(m, sizeof(m), std::fabs(dv), 1);
        snprintf(t, sizeof(t), "%s s %s " SYM_DOT " " SYM_AVG " %s%s km/h", n, gained >= 0 ? "gewonnen" : "verloren",
                 dv >= 0 ? "+" : "\xE2\x80\x93", m);
        col = std::fabs(dv) < TREND_MIN_KMH ? theme::MUTED : (gained > 0 ? theme::GOOD : theme::WARN);
      }
    }
    setText(compete_, shownCompete_, sizeof(shownCompete_), t);
    if (col != shownCompeteColor_) {
      shownCompeteColor_ = col;
      lv_obj_set_style_text_color(compete_, theme::c(col), 0);
    }
  }

  void makeBar(lv_obj_t* parent, int i, const char* label, bool bipolar) {
    Bar& b = bars_[i];
    b.bipolar = bipolar;
    const int32_t w = BOARD_LCD_HOR_RES - BAR_X - BAR_RIGHT;
    b.label = theme::label(parent, &font_m12, true, label);
    lv_obj_set_pos(b.label, BAR_X, BAR_TOPS[i]);
    b.value = theme::label(parent, &font_m14, false, fmt::NO_VALUE);
    lv_obj_set_width(b.value, w);
    lv_obj_set_style_text_align(b.value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(b.value, BAR_X, BAR_TOPS[i] - 1);
    b.track = lv_obj_create(parent);
    lv_obj_remove_style_all(b.track);
    lv_obj_set_pos(b.track, BAR_X, BAR_TOPS[i] + 18);
    lv_obj_set_size(b.track, w, BAR_H);
    lv_obj_set_style_radius(b.track, 3, 0);
    lv_obj_set_style_bg_color(b.track, theme::c(theme::SURFACE), 0);
    lv_obj_set_style_bg_opa(b.track, LV_OPA_COVER, 0);
    lv_obj_remove_flag(b.track, LV_OBJ_FLAG_CLICKABLE);
    if (bipolar) {
      lv_obj_t* mid = lv_obj_create(parent);
      lv_obj_remove_style_all(mid);
      lv_obj_set_pos(mid, BAR_X + w / 2, BAR_TOPS[i] + 16);
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
    if (bipolar) {
      b.mark = lv_obj_create(parent);
      lv_obj_remove_style_all(b.mark);
      lv_obj_set_size(b.mark, 2, BAR_H + 6);
      lv_obj_set_style_bg_color(b.mark, theme::c(theme::GOOD), 0);
      lv_obj_set_style_bg_opa(b.mark, LV_OPA_COVER, 0);
      lv_obj_add_flag(b.mark, LV_OBJ_FLAG_HIDDEN);
    }
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

  // Marke "bester Wert" auf dem Beschleunigungsbalken (frac 0–1 rechts der Mitte, NAN = aus)
  void setMark(int i, float frac) {
    Bar& b = bars_[i];
    if (!b.mark) return;
    const int32_t w = BOARD_LCD_HOR_RES - BAR_X - BAR_RIGHT;
    const int32_t x = std::isnan(frac) ? -1 : BAR_X + w / 2 + static_cast<int32_t>(std::lround((frac > 1 ? 1 : frac) * w / 2)) - 1;
    if (x == b.shownMark) return;
    b.shownMark = x;
    if (x < 0) {
      lv_obj_add_flag(b.mark, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_remove_flag(b.mark, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(b.mark, x, BAR_TOPS[i] + 15);
    }
  }

  static void setText(lv_obj_t* l, char* shown, size_t size, const char* t) {
    if (strcmp(shown, t) == 0) return;
    snprintf(shown, size, "%s", t);
    lv_label_set_text(l, t);
  }

  // ---------- Live-Diagramm im Fenster (Tippen auf den Bogen) ----------
  static void onGaugeTap(lv_event_t* e) {
    if (overlay::isOpen()) return;
    static_cast<SportPage*>(lv_event_get_user_data(e))->openChart();
  }

  void openChart() {
    lv_obj_t* card = overlay::open("");
    chartGen_ = overlay::generation();
    lv_obj_set_layout(card, LV_LAYOUT_NONE);
    lv_obj_t* h = theme::label(card, &font_m14, false, "Letzte 30 s");
    lv_obj_set_pos(h, 0, 0);
    // Serienpaar umschalten
    lv_obj_t* chip = lv_obj_create(card);
    lv_obj_remove_style_all(chip);
    lv_obj_set_size(chip, LV_SIZE_CONTENT, 24);
    lv_obj_set_style_radius(chip, 12, 0);
    lv_obj_set_style_border_width(chip, 1, 0);
    lv_obj_set_style_border_color(chip, theme::c(theme::LINE), 0);
    lv_obj_set_style_bg_color(chip, theme::c(theme::LINE), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_pad_hor(chip, 10, 0);
    lv_obj_set_flex_flow(chip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(chip, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(chip, 6, 0);
    lv_obj_add_flag(chip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(chip, 6);
    lv_obj_add_event_cb(chip, onChip, LV_EVENT_SHORT_CLICKED, this);
    chipA_ = theme::label(chip, &font_m14, false, "");
    lv_obj_set_style_text_color(chipA_, theme::c(theme::ACCENT), 0);
    chipB_ = theme::label(chip, &font_m14, true, "");
    lv_obj_align(chip, LV_ALIGN_TOP_RIGHT, 0, -4);
    chart_ = lv_obj_create(card);
    lv_obj_remove_style_all(chart_);
    lv_obj_set_pos(chart_, 0, 30);
    lv_obj_set_size(chart_, LV_PCT(100), CH_Y0 + CH_H + 8);
    lv_obj_add_flag(chart_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(chart_, onChartTap, LV_EVENT_SHORT_CLICKED, nullptr);  // Tippen ins Diagramm schließt
    lv_obj_add_event_cb(chart_, onChartDraw, LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_t* note = theme::label(card, &font_m12, true, "Tippen ins Diagramm schließt");
    lv_obj_align(note, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    overlay::setOnClose([] { self->chart_ = nullptr; });
    refreshChip();
  }

  void refreshChip() {
    if (!chipA_) return;
    char t[32];
    snprintf(t, sizeof(t), "\xE2\x80\x94 %s", live::label(PAIRS[pair()][0]));  // —
    lv_label_set_text(chipA_, t);
    snprintf(t, sizeof(t), "- - %s", live::label(PAIRS[pair()][1]));
    lv_label_set_text(chipB_, t);
    if (chart_) lv_obj_invalidate(chart_);
  }

  static void onChip(lv_event_t* e) {
    auto* p = static_cast<SportPage*>(lv_event_get_user_data(e));
    uiprefs::get().sportPair = static_cast<uint8_t>((pair() + 1) % 3);
    uiprefs::save();
    p->refreshChip();
  }

  static void onChartTap(lv_event_t*) { overlay::close(); }

  static void onChartDraw(lv_event_t* e) {
    lv_area_t a;
    lv_obj_get_coords(lv_event_get_target_obj(e), &a);
    drawLive(lv_event_get_layer(e), a.x1 + CH_X0, a.x1 + CH_X1, a.y1 + CH_Y0, CH_H);
  }

  // ---------- Drehzahlbogen ----------
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
      line(layer, cx + std::lround((R - 6) * c), cy + std::lround((R - 6) * sn), cx + std::lround((R + 7) * c),
           cy + std::lround((R + 7) * sn), theme::BG, 2);
      char t[4];
      snprintf(t, sizeof(t), "%d", k);
      text(layer, t, cx + std::lround((R - 17) * c) - 8, cy + std::lround((R - 17) * sn) - 8, 16, LV_TEXT_ALIGN_CENTER,
           theme::MUTED, &font_m12);
    }
  }

  lv_obj_t* gauge_ = nullptr;
  lv_obj_t* speed_ = nullptr;
  lv_obj_t* gearRow_ = nullptr;
  lv_obj_t* arrow_ = nullptr;
  lv_obj_t* gear_ = nullptr;
  lv_obj_t* compete_ = nullptr;
  lv_obj_t* chart_ = nullptr;
  lv_obj_t* chipA_ = nullptr;
  lv_obj_t* chipB_ = nullptr;
  uint32_t chartGen_ = 0xFFFFFFFF;
  uint32_t chartDraw_ = 0;
  FieldBar fields_;
  Bar bars_[4];
  char shownSpeed_[12] = "";
  char shownGear_[8] = "";
  char shownCompete_[48] = "";
  uint32_t shownCompeteColor_ = 0xFFFFFFFF;
  bool shownArrow_ = false;
  bool shownImu_ = false;
  uint32_t shiftSince_ = 0;
  int drawnRpm_ = -2;
  uint32_t drawnSeq_ = 0;
  uint32_t lastDraw_ = 0;
  // Wettbewerb
  float trendKm_[TREND_SAMPLES] = {};
  float trendDur_[TREND_SAMPLES] = {};
  int trendHead_ = 0, trendCount_ = 0;
  uint32_t trendAt_ = 0;
  float accBest_ = NAN;   // beste Beschleunigung abgeschlossener Phasen
  float phaseMax_ = NAN;  // laufende Phase
  uint16_t seenResult_ = 0;
  bool seenInit_ = false;
  uint32_t resultAt_ = 0;
  CarSnapshot last_;
};

}  // namespace

Page* sportPage() {
  static SportPage page;
  return &page;
}
