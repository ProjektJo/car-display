// Seite "Sprint" (A10, U Seite 3): Stoppuhr 0–100 mit Status bereit / läuft / geschafft und
// Fortschrittsbalken, Tempoverlauf der letzten und der besten Messung, Tabelle 0–50 / 0–100 / 80–120
// mit letzter und bester Zeit und vier Kacheln mit Fahrtwerten. Positionen aus der Vorschau (pSprint).
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "config.h"
#include "page.h"
#include "ui/symbols.h"
#include "ui/theme.h"
#include "util/format.h"

namespace {

// Karte links
constexpr int32_t CARD_X = 8, CARD_Y = 6, CARD_W = 150, CARD_H = 96;
constexpr uint32_t DONE_SHOW_MS = 10000;  // "geschafft" 10 s lang (Vorschau)
// Verlauf rechts
constexpr int32_t GX0 = 186, GX1 = 308, GY0 = 14, GH = 72;
constexpr float G_MIN_TMAX_S = 14.0f;
constexpr int DASH_PX = 3;
// Tabelle und Kacheln
constexpr int32_t TABLE_X = 12, TABLE_Y = 106, ROW_H = 19, COL_W = 44, COL_GAP = 18;
constexpr int32_t TILE_SIDE = 8, TILE_GAP = 5, TILE_BOTTOM = 5, TILE_H = 30;

void line(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color, int32_t w) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.p1.x = x1;
  d.p1.y = y1;
  d.p2.x = x2;
  d.p2.y = y2;
  d.color = theme::c(color);
  d.width = w;
  lv_draw_line(layer, &d);
}

void text(lv_layer_t* layer, const char* t, int32_t x, int32_t y, int32_t w, lv_text_align_t al, uint32_t color) {
  lv_draw_label_dsc_t d;
  lv_draw_label_dsc_init(&d);
  d.text = t;
  d.text_local = 1;
  d.font = &font_small;
  d.color = theme::c(color);
  d.align = al;
  lv_area_t a = {x, y, x + w - 1, y + 12};
  lv_draw_label(layer, &d, &a);
}

void secText(char* out, size_t size, float s) {
  char n[12];
  fmt::number(n, sizeof(n), s, 1);
  snprintf(out, size, std::isnan(s) ? "%s" : "%s s", n);
}

class SprintPage : public Page {
 public:
  SprintPage() : Page("Sprint") {}

  void create(lv_obj_t* parent) override {
    graph_ = lv_obj_create(parent);
    lv_obj_remove_style_all(graph_);
    lv_obj_set_size(graph_, BOARD_LCD_HOR_RES, TABLE_Y);
    lv_obj_remove_flag(graph_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(graph_, onDraw, LV_EVENT_DRAW_MAIN, this);

    // Karte 0–100
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_pos(card, CARD_X, CARD_Y);
    lv_obj_set_size(card, CARD_W, CARD_H);
    lv_obj_set_style_bg_color(card, theme::c(theme::SURFACE), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, theme::RADIUS_TILE, 0);
    lv_obj_set_style_pad_hor(card, 10, 0);
    lv_obj_set_style_pad_ver(card, 6, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    status_ = theme::label(card, &font_m12, true, "");
    lv_obj_set_pos(status_, 0, 0);
    lv_obj_t* row = lv_obj_create(card);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(row, 0, 14);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(row, 3, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
    big38_ = lv_font_montserrat_38;
    big38_.fallback = &font_m28;
    time_ = theme::label(row, &big38_, false, fmt::NO_VALUE);
    lv_obj_t* s = theme::label(row, &font_m12, true, "s");
    lv_obj_set_style_pad_bottom(s, 6, 0);
    track_ = lv_obj_create(card);
    lv_obj_remove_style_all(track_);
    lv_obj_set_pos(track_, 0, 62);
    lv_obj_set_size(track_, CARD_W - 20, 5);
    lv_obj_set_style_radius(track_, 3, 0);
    lv_obj_set_style_bg_color(track_, theme::c(theme::BG), 0);
    lv_obj_set_style_bg_opa(track_, LV_OPA_COVER, 0);
    fill_ = lv_obj_create(track_);
    lv_obj_remove_style_all(fill_);
    lv_obj_set_size(fill_, 0, 5);
    lv_obj_set_style_radius(fill_, 3, 0);
    lv_obj_set_style_bg_opa(fill_, LV_OPA_COVER, 0);
    note_ = theme::label(card, &font_small, true, "");
    lv_obj_set_pos(note_, 0, 71);

    // Tabelle
    lv_obj_t* h = theme::label(parent, &font_small, true, "Messung");
    lv_obj_set_pos(h, TABLE_X, TABLE_Y);
    colLabel(parent, "letzte", 0, TABLE_Y);
    colLabel(parent, "beste", 1, TABLE_Y);
    static const char* const NAMES[3] = {"0" "\xE2\x80\x93" "50 km/h", "0" "\xE2\x80\x93" "100 km/h", "80" "\xE2\x80\x93" "120 km/h"};
    for (int i = 0; i < 3; i++) {
      const int32_t y = TABLE_Y + 14 + i * ROW_H;
      lv_obj_t* sep = lv_obj_create(parent);
      lv_obj_remove_style_all(sep);
      lv_obj_set_pos(sep, TABLE_X, y);
      lv_obj_set_size(sep, BOARD_LCD_HOR_RES - 2 * TABLE_X, 1);
      lv_obj_set_style_bg_color(sep, theme::c(theme::LINE), 0);
      lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
      lv_obj_t* n = theme::label(parent, &font_m12, false, NAMES[i]);
      lv_obj_set_pos(n, TABLE_X, y + 3);
      last_[i] = colLabel(parent, fmt::NO_VALUE, 0, y + 3, false);
      best_[i] = colLabel(parent, fmt::NO_VALUE, 1, y + 3, false);
      lv_obj_set_style_text_color(best_[i], theme::c(theme::GOOD), 0);
    }

    // Kacheln: Ø Fahrt, Zeit/100 km, Vmax, Spitze kW
    static const char* const TILE_LABELS[4] = {SYM_AVG " Fahrt", "Zeit/100 km", "Vmax", "Spitze"};
    const int32_t w = (BOARD_LCD_HOR_RES - 2 * TILE_SIDE - 3 * TILE_GAP) / 4;
    for (int i = 0; i < 4; i++) {
      lv_obj_t* t = lv_obj_create(parent);
      lv_obj_remove_style_all(t);
      lv_obj_set_size(t, w, TILE_H);
      lv_obj_set_pos(t, TILE_SIDE + i * (w + TILE_GAP), theme::CONTENT_H - TILE_BOTTOM - TILE_H);
      lv_obj_set_style_bg_color(t, theme::c(theme::SURFACE), 0);
      lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
      lv_obj_set_style_radius(t, theme::RADIUS_TILE, 0);
      lv_obj_set_style_pad_hor(t, 6, 0);
      lv_obj_set_style_pad_ver(t, 2, 0);
      lv_obj_set_flex_flow(t, LV_FLEX_FLOW_COLUMN);
      lv_obj_remove_flag(t, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_remove_flag(t, LV_OBJ_FLAG_SCROLLABLE);
      theme::label(t, &font_small, true, TILE_LABELS[i]);
      tile_[i] = theme::label(t, &font_m12, false, fmt::NO_VALUE);
    }
  }

  void update(const CarSnapshot& s) override {
    snap_ = s;
    const SprintInfo& sp = s.sprint;
    const bool running = sp.state == perf::State::Running;
    const bool done = sp.state == perf::State::Done && s.now - sp.doneAtMs < DONE_SHOW_MS;
    char t[48];
    snprintf(t, sizeof(t), "0" "\xE2\x80\x93" "100 km/h " SYM_DOT " %s", running ? "läuft" : done ? "geschafft" : "bereit");
    setText(status_, shownStatus_, sizeof(shownStatus_), t);
    const uint32_t c = running ? theme::ACCENT : done ? theme::GOOD : theme::TEXT;
    if (c != shownColor_) {
      shownColor_ = c;
      lv_obj_set_style_text_color(time_, theme::c(c), 0);
      lv_obj_set_style_text_color(status_, theme::c(running || done ? c : theme::MUTED), 0);
      lv_obj_set_style_bg_color(fill_, theme::c(done ? theme::GOOD : theme::ACCENT), 0);
    }
    const float tm = running ? sp.elapsed : sp.last100;
    fmt::number(t, sizeof(t), tm, 1);
    setText(time_, shownTime_, sizeof(shownTime_), t);
    const float v = s.speed.get(s.now);
    float frac = (running || done) && !std::isnan(v) ? v / 100.0f : 0.0f;
    frac = frac < 0 ? 0 : (frac > 1 ? 1 : frac);
    const int32_t fw = static_cast<int32_t>(std::lround(frac * (CARD_W - 20)));
    if (fw != shownFill_) {
      shownFill_ = fw;
      lv_obj_set_width(fill_, fw);
    }
    if (running || done) {
      char n[12];
      fmt::number(n, sizeof(n), v, 0);
      snprintf(t, sizeof(t), "%s km/h", n);
    } else {
      snprintf(t, sizeof(t), "Startet aus dem Stand");
    }
    setText(note_, shownNote_, sizeof(shownNote_), t);

    // Tabelle: letzte und beste Zeit
    const float lasts[3] = {sp.last50, running ? NAN : sp.last100, sp.last80120};
    const float bests[3] = {sp.best50, sp.best100, sp.best80120};
    for (int i = 0; i < 3; i++) {
      secText(t, sizeof(t), lasts[i]);
      setText(last_[i], shownLast_[i], sizeof(shownLast_[i]), t);
      secText(t, sizeof(t), bests[i]);
      setText(best_[i], shownBest_[i], sizeof(shownBest_[i]), t);
    }

    // Kacheln
    const float km = s.tripKm.get(s.now), dur = s.tripDurationS.get(s.now);
    const float vAvg = (!std::isnan(km) && !std::isnan(dur) && dur > 60) ? km / (dur / 3600.0f) : NAN;
    char n[12];
    fmt::number(n, sizeof(n), vAvg, 0);
    snprintf(t, sizeof(t), std::isnan(vAvg) ? "%s" : "%s km/h", n);
    setText(tile_[0], shownTile_[0], sizeof(shownTile_[0]), t);
    if (!std::isnan(vAvg) && vAvg >= 1) {
      const float h = 100.0f / vAvg;  // Zeit pro 100 km = 100 ÷ Ø Fahrt (A10)
      int hh = static_cast<int>(h), mm = static_cast<int>(std::lround((h - hh) * 60));
      if (mm == 60) {
        hh++;
        mm = 0;
      }
      snprintf(t, sizeof(t), "%d:%02d h", hh, mm);
    } else {
      snprintf(t, sizeof(t), "%s", fmt::NO_VALUE);
    }
    setText(tile_[1], shownTile_[1], sizeof(shownTile_[1]), t);
    const float vmax = s.tripVmax.get(s.now);
    fmt::number(n, sizeof(n), vmax, 0);
    snprintf(t, sizeof(t), std::isnan(vmax) ? "%s" : "%s km/h", n);
    setText(tile_[2], shownTile_[2], sizeof(shownTile_[2]), t);
    const float kw = s.tripKwPeak.get(s.now);
    fmt::number(n, sizeof(n), kw, 0);
    snprintf(t, sizeof(t), std::isnan(kw) ? "%s" : "%s kW", n);
    setText(tile_[3], shownTile_[3], sizeof(shownTile_[3]), t);

    // Verlauf neu zeichnen, wenn sich die Messung geändert hat (höchstens 5 Hz)
    const uint32_t sig = sp.lastTrace.count * 1000u + sp.bestTrace.count + static_cast<uint32_t>(sp.state) * 100000u;
    if (sig != drawnSig_ && s.now - lastDraw_ >= cfg::CHART_MIN_REDRAW_MS) {
      drawnSig_ = sig;
      lastDraw_ = s.now;
      lv_obj_invalidate(graph_);
    }
  }

  void onShow() override { lv_obj_invalidate(graph_); }

 private:
  lv_obj_t* colLabel(lv_obj_t* parent, const char* text, int col, int32_t y, bool small = true) {
    lv_obj_t* l = theme::label(parent, small ? &font_small : &font_m12, small, text);
    lv_obj_set_width(l, COL_W);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(l, BOARD_LCD_HOR_RES - TABLE_X - (2 - col) * COL_W - (1 - col) * COL_GAP, y);
    return l;
  }

  static void setText(lv_obj_t* l, char* shown, size_t size, const char* t) {
    if (strcmp(shown, t) == 0) return;
    snprintf(shown, size, "%s", t);
    lv_label_set_text(l, t);
  }

  static void onDraw(lv_event_t* e) {
    static_cast<SprintPage*>(lv_event_get_user_data(e))->draw(lv_event_get_layer(e), lv_event_get_target_obj(e));
  }

  void drawTrace(lv_layer_t* layer, const perf::Trace& tr, int32_t ox, int32_t oy, float tMax, uint32_t col, bool dashed) {
    int32_t px = 0, py = 0;
    for (int i = 0; i < tr.count; i++) {
      const int32_t x = ox + GX0 + static_cast<int32_t>(std::lround(tr.timeAt(i) / tMax * (GX1 - GX0)));
      const int32_t y = oy + GY0 + GH - static_cast<int32_t>(std::lround(i * perf::TRACE_STEP_KMH / 100.0f * GH));
      if (i > 0 && (!dashed || i % 2 == 1)) line(layer, px, py, x, y, col, 2);
      px = x;
      py = y;
    }
  }

  void draw(lv_layer_t* layer, lv_obj_t* obj) {
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    const SprintInfo& sp = snap_.sprint;
    float tMax = G_MIN_TMAX_S;
    for (const perf::Trace* tr : {&sp.lastTrace, &sp.bestTrace})
      if (tr->count > 0 && tr->timeAt(tr->count - 1) > tMax) tMax = tr->timeAt(tr->count - 1);
    char t[16];
    for (int v = 0; v <= 100; v += 50) {
      const int32_t y = a.y1 + GY0 + GH - v * GH / 100;
      line(layer, a.x1 + GX0, y, a.x1 + GX1, y, theme::LINE, 1);
      snprintf(t, sizeof(t), "%d", v);
      text(layer, t, a.x1 + GX0 - 24, y - 6, 20, LV_TEXT_ALIGN_RIGHT, theme::MUTED);
    }
    drawTrace(layer, sp.bestTrace, a.x1, a.y1, tMax, theme::GOOD, true);
    drawTrace(layer, sp.lastTrace, a.x1, a.y1, tMax, theme::ACCENT, false);
    text(layer, "0", a.x1 + GX0, a.y1 + GY0 + GH + 2, 20, LV_TEXT_ALIGN_LEFT, theme::MUTED);
    fmt::number(t, sizeof(t), tMax, 0);
    char u[20];
    snprintf(u, sizeof(u), "%s s", t);
    text(layer, u, a.x1 + GX1 - 40, a.y1 + GY0 + GH + 2, 40, LV_TEXT_ALIGN_RIGHT, theme::MUTED);
    text(layer, "km/h " SYM_DOT, a.x1 + GX0, a.y1 + GY0 - 13, 40, LV_TEXT_ALIGN_LEFT, theme::MUTED);
    text(layer, "letzte", a.x1 + GX0 + 34, a.y1 + GY0 - 13, 40, LV_TEXT_ALIGN_LEFT, theme::ACCENT);
    text(layer, SYM_DOT " beste", a.x1 + GX0 + 64, a.y1 + GY0 - 13, 50, LV_TEXT_ALIGN_LEFT, theme::GOOD);
  }

  lv_font_t big38_;
  lv_obj_t* graph_ = nullptr;
  lv_obj_t* status_ = nullptr;
  lv_obj_t* time_ = nullptr;
  lv_obj_t* track_ = nullptr;
  lv_obj_t* fill_ = nullptr;
  lv_obj_t* note_ = nullptr;
  lv_obj_t* last_[3] = {};
  lv_obj_t* best_[3] = {};
  lv_obj_t* tile_[4] = {};
  char shownStatus_[48] = "";
  char shownTime_[16] = "";
  char shownNote_[32] = "";
  char shownLast_[3][16] = {};
  char shownBest_[3][16] = {};
  char shownTile_[4][20] = {};
  uint32_t shownColor_ = 0xFFFFFFFF;
  int32_t shownFill_ = -1;
  uint32_t drawnSig_ = 0xFFFFFFFF;
  uint32_t lastDraw_ = 0;
  CarSnapshot snap_;
};

}  // namespace

Page* sprintPage() {
  static SprintPage page;
  return &page;
}
