// Seite "Sprint" (A10, U Seite 3), umgebaut am 7.10.2026 nach der ersten Fahrt (Jos Wunsch):
// Das Zeit/Tempo-Diagramm ist groß. Im Stand zeigt ein grünes Feld "READY"; beim Anfahren wird es zu
// einem kleinen orangen "GO" in der Ecke und die Live-Kurve läuft orange mit (jedes Anfahren aus dem Stand,
// gezählt wird nur ein erkannter Sprint). Nach dem Ziel zeigt das Feld die Zeit in Grün. Die Messwerte
// 0–50 / 0–100 / 80–120 stehen klein unten; Tippen zeigt einen groß mit Bestzeit und Abstand.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "config.h"
#include "page.h"
#include "ui/overlay.h"
#include "ui/symbols.h"
#include "ui/theme.h"
#include "util/format.h"

namespace {

// Diagramm
constexpr int32_t GX0 = 34, GX1 = 310, GY0 = 8, GH = 158;
constexpr float G_MIN_TMAX_S = 12.0f;
constexpr uint32_t DONE_SHOW_MS = 10000;  // Ergebnis 10 s lang im Feld
// Chips unten
constexpr int32_t CHIP_SIDE = 8, CHIP_GAP = 6, CHIP_BOTTOM = 5, CHIP_H = 40;
// Feld READY / GO
constexpr int32_t READY_W = 150, READY_H = 56, GO_W = 64, GO_H = 26;

const char* const NAMES[3] = {"0" "\xE2\x80\x93" "50", "0" "\xE2\x80\x93" "100", "80" "\xE2\x80\x93" "120"};

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
  d.round_end = d.round_start = 1;
  lv_draw_line(layer, &d);
}

void text(lv_layer_t* layer, const char* t, int32_t x, int32_t y, int32_t w, lv_text_align_t al, uint32_t color,
          const lv_font_t* font = &font_m12) {
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

void secText(char* out, size_t size, float s, bool unit = true) {
  char n[12];
  fmt::number(n, sizeof(n), s, 1);
  snprintf(out, size, (std::isnan(s) || !unit) ? "%s" : "%s s", n);
}

class SprintPage;
SprintPage* self = nullptr;

class SprintPage : public Page {
 public:
  SprintPage() : Page("Sprint") { self = this; }

  void create(lv_obj_t* parent) override {
    graph_ = lv_obj_create(parent);
    lv_obj_remove_style_all(graph_);
    lv_obj_set_size(graph_, BOARD_LCD_HOR_RES, GY0 + GH + 16);
    lv_obj_remove_flag(graph_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(graph_, onDraw, LV_EVENT_DRAW_MAIN, this);

    // Laufende Zeit groß oben links im Diagramm (dort ist die Kurve noch nicht)
    timeRow_ = lv_obj_create(parent);
    lv_obj_remove_style_all(timeRow_);
    lv_obj_set_size(timeRow_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_pos(timeRow_, GX0 + 8, GY0 + 4);
    lv_obj_set_flex_flow(timeRow_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(timeRow_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(timeRow_, 3, 0);
    lv_obj_remove_flag(timeRow_, LV_OBJ_FLAG_CLICKABLE);
    time_ = theme::label(timeRow_, &font_v32, false, "");
    lv_obj_t* s = theme::label(timeRow_, &font_m14, true, "s");
    lv_obj_set_style_pad_bottom(s, 6, 0);
    sub_ = theme::label(parent, &font_m12, true, "");
    lv_obj_set_pos(sub_, GX0 + 8, GY0 + 42);

    // Feld READY (groß, Mitte) bzw. GO (klein, oben rechts) bzw. Ergebnis
    badge_ = lv_obj_create(parent);
    lv_obj_remove_style_all(badge_);
    lv_obj_set_style_radius(badge_, 10, 0);
    lv_obj_set_style_bg_opa(badge_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(badge_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(badge_, LV_OBJ_FLAG_SCROLLABLE);
    badgeText_ = theme::label(badge_, &font_v32, false, "");
    lv_obj_set_style_text_color(badgeText_, theme::c(theme::BG), 0);
    lv_obj_center(badgeText_);

    // Chips: 0–50 / 0–100 / 80–120 mit letzter Zeit, Bestzeit klein
    const int32_t w = (BOARD_LCD_HOR_RES - 2 * CHIP_SIDE - 2 * CHIP_GAP) / 3;
    for (int i = 0; i < 3; i++) {
      lv_obj_t* t = lv_obj_create(parent);
      lv_obj_remove_style_all(t);
      lv_obj_set_size(t, w, CHIP_H);
      lv_obj_set_pos(t, CHIP_SIDE + i * (w + CHIP_GAP), theme::CONTENT_H - CHIP_BOTTOM - CHIP_H);
      lv_obj_set_style_bg_color(t, theme::c(theme::SURFACE), 0);
      lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
      lv_obj_set_style_bg_color(t, theme::c(theme::LINE), LV_STATE_PRESSED);
      lv_obj_set_style_radius(t, theme::RADIUS_TILE, 0);
      lv_obj_set_style_pad_hor(t, 6, 0);
      lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_remove_flag(t, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_event_cb(t, onChip, LV_EVENT_SHORT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
      lv_obj_t* n = theme::label(t, &font_m12, true, NAMES[i]);
      lv_obj_align(n, LV_ALIGN_TOP_LEFT, 0, 3);
      best_[i] = theme::label(t, &font_m12, false, "");
      lv_obj_set_style_text_color(best_[i], theme::c(theme::GOOD), 0);
      lv_obj_align(best_[i], LV_ALIGN_TOP_RIGHT, 0, 3);
      last_[i] = theme::label(t, &font_m20, false, fmt::NO_VALUE);
      lv_obj_align(last_[i], LV_ALIGN_BOTTOM_MID, 0, -2);
    }
  }

  void update(const CarSnapshot& s) override {
    snap_ = s;
    const SprintInfo& sp = s.sprint;
    const bool running = sp.state == perf::State::Running;
    const bool live = sp.state == perf::State::Waiting;
    const bool done = sp.state == perf::State::Done && s.now - sp.doneAtMs < DONE_SHOW_MS;
    const bool newBest = done && !std::isnan(sp.best100) && std::fabs(sp.best100 - sp.last100) < 0.005f &&
                         sp.resultKind == perf::Kind::S100 && (std::isnan(sp.resultPrevBest) || sp.last100 <= sp.resultPrevBest);

    // Feld: READY im Stand, GO beim Anfahren, Ergebnis nach dem Ziel
    int mode;  // 0 aus, 1 READY, 2 GO, 3 LIVE, 4 Ergebnis
    if (done) mode = 4;
    else if (running) mode = 2;
    else if (live) mode = 3;
    else if (sp.standing && s.engineRunning()) mode = 1;
    else mode = 0;
    char t[48];
    if (mode == 4) {
      secText(t, sizeof(t), sp.last100);
      if (newBest) {
        char b[32];
        snprintf(b, sizeof(b), "%s " SYM_DOT " Best", t);
        snprintf(t, sizeof(t), "%s", b);
      }
    }
    if (mode != shownMode_ || (mode == 4 && strcmp(t, shownBadge_) != 0)) {
      shownMode_ = mode;
      if (mode == 0) {
        lv_obj_add_flag(badge_, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_remove_flag(badge_, LV_OBJ_FLAG_HIDDEN);
        const char* bt = mode == 1 ? "READY" : mode == 2 ? "GO" : mode == 3 ? "LIVE" : t;
        snprintf(shownBadge_, sizeof(shownBadge_), "%s", bt);
        lv_label_set_text(badgeText_, shownBadge_);
        const bool small = mode == 2 || mode == 3;
        lv_obj_set_style_text_font(badgeText_, small ? &font_m14 : (mode == 4 ? &font_m20 : &font_v32), 0);
        lv_obj_set_style_bg_color(badge_, theme::c(mode == 1 || mode == 4 ? theme::GOOD : (mode == 2 ? theme::WARN : theme::LINE)), 0);
        lv_obj_set_style_text_color(badgeText_, theme::c(mode == 3 ? theme::TEXT : theme::BG), 0);
        if (small) {
          lv_obj_set_size(badge_, GO_W, GO_H);
          lv_obj_set_pos(badge_, GX1 - GO_W - 4, GY0 + 4);
        } else {
          lv_obj_set_size(badge_, mode == 4 ? READY_W + 20 : READY_W, mode == 4 ? 40 : READY_H);
          lv_obj_set_pos(badge_, (GX0 + GX1) / 2 - (mode == 4 ? READY_W + 20 : READY_W) / 2 + (mode == 4 ? 30 : 0),
                         mode == 4 ? GY0 + GH - 40 - 12 : GY0 + (GH - READY_H) / 2);
        }
        lv_obj_center(badgeText_);
      }
    }

    // Laufende Zeit groß (läuft auch bei der Live-Kurve), sonst die letzte 0–100
    const float tm = (running || live) ? sp.elapsed : (done ? sp.last100 : NAN);
    if (std::isnan(tm)) {
      t[0] = '\0';
    } else {
      fmt::number(t, sizeof(t), tm, 1);
    }
    setText(time_, shownTime_, sizeof(shownTime_), t);
    if (std::isnan(tm)) lv_obj_add_flag(timeRow_, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(timeRow_, LV_OBJ_FLAG_HIDDEN);
    const uint32_t tc = running ? theme::WARN : (done ? theme::GOOD : theme::TEXT);
    if (tc != shownTimeColor_) {
      shownTimeColor_ = tc;
      lv_obj_set_style_text_color(time_, theme::c(tc), 0);
    }
    if (running || live) {
      char n[12];
      fmt::number(n, sizeof(n), s.speed.get(s.now), 0);
      snprintf(t, sizeof(t), "%s km/h%s", n, live ? " " SYM_DOT " zählt nicht" : "");
    } else if (mode == 1) {
      snprintf(t, sizeof(t), "Vollgas aus dem Stand");
    } else if (mode == 0 && !done) {
      snprintf(t, sizeof(t), "Startet aus dem Stand");
    } else {
      t[0] = '\0';
    }
    setText(sub_, shownSub_, sizeof(shownSub_), t);

    // Chips: letzte Zeit groß, beste klein grün
    const float lasts[3] = {sp.last50, running ? NAN : sp.last100, sp.last80120};
    const float bests[3] = {sp.best50, sp.best100, sp.best80120};
    for (int i = 0; i < 3; i++) {
      secText(t, sizeof(t), lasts[i]);
      setText(last_[i], shownLast_[i], sizeof(shownLast_[i]), t);
      if (std::isnan(bests[i])) {
        t[0] = '\0';
      } else {
        char n[12];
        secText(n, sizeof(n), bests[i], false);
        snprintf(t, sizeof(t), "Best %s", n);
      }
      setText(best_[i], shownBest_[i], sizeof(shownBest_[i]), t);
    }

    // Verlauf neu zeichnen, wenn sich die Messung geändert hat (höchstens 5 Hz)
    const uint32_t sig = sp.lastTrace.count * 1000u + sp.bestTrace.count + static_cast<uint32_t>(sp.state) * 100000u +
                         ((running || live) ? static_cast<uint32_t>(sp.elapsed * 5) * 1000000u : 0);
    if (sig != drawnSig_ && s.now - lastDraw_ >= cfg::CHART_MIN_REDRAW_MS) {
      drawnSig_ = sig;
      lastDraw_ = s.now;
      lv_obj_invalidate(graph_);
    }
    updateDetail(s);
  }

  void onShow() override { lv_obj_invalidate(graph_); }

 private:
  static void setText(lv_obj_t* l, char* shown, size_t size, const char* t) {
    if (strcmp(shown, t) == 0) return;
    snprintf(shown, size, "%s", t);
    lv_label_set_text(l, t);
  }

  // ---------- Detail einer Messung (Tippen auf einen Chip) ----------
  static void onChip(lv_event_t* e) {
    if (overlay::isOpen()) return;
    self->openDetail(static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e))));
  }
  static void onDetailClose(lv_event_t*) { overlay::close(); }

  void openDetail(int i) {
    char title[32];
    snprintf(title, sizeof(title), "%s km/h", NAMES[i]);
    lv_obj_t* card = overlay::open("");
    detailGen_ = overlay::generation();
    detailIdx_ = i;
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, onDetailClose, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(lv_obj_get_parent(card), onDetailClose, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_layout(card, LV_LAYOUT_NONE);
    lv_obj_t* t = theme::label(card, &font_m14, false, title);
    lv_obj_set_pos(t, 0, 0);
    lv_obj_t* l = theme::label(card, &font_m12, true, "letzte Messung");
    lv_obj_set_pos(l, 0, 24);
    dLast_ = theme::label(card, &font_v40, false, "");
    lv_obj_set_pos(dLast_, 0, 38);
    lv_obj_t* b = theme::label(card, &font_m12, true, "beste");
    lv_obj_set_pos(b, 160, 24);
    dBest_ = theme::label(card, &font_v40, false, "");
    lv_obj_set_style_text_color(dBest_, theme::c(theme::GOOD), 0);
    lv_obj_set_pos(dBest_, 160, 38);
    dDiff_ = theme::label(card, &font_m20, false, "");
    lv_obj_set_pos(dDiff_, 0, 96);
    lv_obj_t* note = theme::label(card, &font_m12, true, "Tippen schließt");
    lv_obj_align(note, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    dShown_[0] = '\0';
    updateDetail(snap_);
  }

  void updateDetail(const CarSnapshot& s) {
    if (!overlay::isOpen() || overlay::generation() != detailGen_) return;
    const SprintInfo& sp = s.sprint;
    const float lasts[3] = {sp.last50, sp.last100, sp.last80120};
    const float bests[3] = {sp.best50, sp.best100, sp.best80120};
    const float l = lasts[detailIdx_], b = bests[detailIdx_];
    char a[16], c[16], d[48];
    secText(a, sizeof(a), l);
    secText(c, sizeof(c), b);
    if (std::isnan(l) || std::isnan(b)) {
      snprintf(d, sizeof(d), "Noch keine Messung");
    } else if (l - b < 0.05f) {
      snprintf(d, sizeof(d), "Das ist deine Bestzeit");
    } else {
      char n[12];
      fmt::number(n, sizeof(n), l - b, 1);
      snprintf(d, sizeof(d), "+%s s zur Bestzeit", n);
    }
    char all[96];
    snprintf(all, sizeof(all), "%s|%s|%s", a, c, d);
    if (strcmp(all, dShown_) == 0) return;
    snprintf(dShown_, sizeof(dShown_), "%s", all);
    lv_label_set_text(dLast_, a);
    lv_label_set_text(dBest_, c);
    lv_label_set_text(dDiff_, d);
    lv_obj_set_style_text_color(dDiff_, theme::c(std::isnan(l) || std::isnan(b) ? theme::MUTED : (l - b < 0.05f ? theme::GOOD : theme::WARN)), 0);
  }

  // ---------- Diagramm ----------
  static void onDraw(lv_event_t* e) {
    static_cast<SprintPage*>(lv_event_get_user_data(e))->draw(lv_event_get_layer(e), lv_event_get_target_obj(e));
  }

  static int32_t px(int32_t ox, float tS, float tMax) {
    return ox + GX0 + static_cast<int32_t>(std::lround(tS / tMax * (GX1 - GX0)));
  }
  static int32_t py(int32_t oy, float v) {
    return oy + GY0 + GH - static_cast<int32_t>(std::lround(v / 100.0f * GH));
  }

  void drawTrace(lv_layer_t* layer, const perf::Trace& tr, int32_t ox, int32_t oy, float tMax, uint32_t col, int32_t w,
                 bool dashed) {
    int32_t lx = 0, ly = 0;
    for (int i = 0; i < tr.count; i++) {
      const int32_t x = px(ox, tr.timeAt(i), tMax);
      const int32_t y = py(oy, i * perf::TRACE_STEP_KMH);
      if (i > 0 && (!dashed || i % 2 == 1)) line(layer, lx, ly, x, y, col, w);
      lx = x;
      ly = y;
    }
  }

  void draw(lv_layer_t* layer, lv_obj_t* obj) {
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    const int32_t ox = a.x1, oy = a.y1;
    const SprintInfo& sp = snap_.sprint;
    const bool moving = sp.state == perf::State::Running || sp.state == perf::State::Waiting;
    float tMax = G_MIN_TMAX_S;
    for (const perf::Trace* tr : {&sp.lastTrace, &sp.bestTrace})
      if (tr->count > 0 && tr->timeAt(tr->count - 1) > tMax) tMax = tr->timeAt(tr->count - 1);
    if (moving && !std::isnan(sp.elapsed) && sp.elapsed + 1 > tMax) tMax = sp.elapsed + 1;
    tMax = std::ceil(tMax / 2) * 2;

    // Raster: Tempo 0/50/100, Zeit alle 2 s (bzw. 5 s)
    char t[16];
    for (int v = 0; v <= 100; v += 25) {
      const int32_t y = py(oy, static_cast<float>(v));
      line(layer, ox + GX0, y, ox + GX1, y, theme::LINE, 1);
      if (v % 50 == 0) {
        snprintf(t, sizeof(t), "%d", v);
        text(layer, t, ox, y - 7, GX0 - 5, LV_TEXT_ALIGN_RIGHT, theme::MUTED);
      }
    }
    const int step = tMax > 20 ? 5 : 2;
    for (int s = step; s <= static_cast<int>(tMax); s += step) {
      const int32_t x = px(ox, static_cast<float>(s), tMax);
      line(layer, x, py(oy, 0), x, py(oy, 0) + 3, theme::MUTED, 1);
      snprintf(t, sizeof(t), "%d", s);
      text(layer, t, x - 15, py(oy, 0) + 3, 30, LV_TEXT_ALIGN_CENTER, theme::MUTED);
    }
    text(layer, "km/h", ox, oy + GY0 - 6, GX0 - 3, LV_TEXT_ALIGN_RIGHT, theme::MUTED);

    // Beste grün gestrichelt, letzte in accent, laufende orange und dicker mit Punkt an der Spitze
    drawTrace(layer, sp.bestTrace, ox, oy, tMax, theme::GOOD, 2, true);
    if (moving) {
      drawTrace(layer, sp.lastTrace, ox, oy, tMax, theme::WARN, 3, false);
      const float v = snap_.speed.get(snap_.now);
      if (!std::isnan(v) && !std::isnan(sp.elapsed) && sp.lastTrace.count > 0) {
        const int32_t x = px(ox, sp.elapsed, tMax), y = py(oy, v > 100 ? 100 : v);
        const int32_t lx = px(ox, sp.lastTrace.timeAt(sp.lastTrace.count - 1), tMax);
        const int32_t ly = py(oy, (sp.lastTrace.count - 1) * perf::TRACE_STEP_KMH);
        const uint32_t col = theme::WARN;
        line(layer, lx, ly, x, y, col, 3);
        lv_draw_rect_dsc_t d;
        lv_draw_rect_dsc_init(&d);
        d.radius = LV_RADIUS_CIRCLE;
        d.bg_color = theme::c(col);
        d.bg_opa = LV_OPA_COVER;
        lv_area_t da = {x - 4, y - 4, x + 4, y + 4};
        lv_draw_rect(layer, &d, &da);
      }
    } else {
      drawTrace(layer, sp.lastTrace, ox, oy, tMax, sp.state == perf::State::Done ? theme::GOOD : theme::ACCENT, 3, false);
    }
    // Legende rechts unten im Diagramm
    text(layer, "\xE2\x80\x94 letzte", ox + GX1 - 120, py(oy, 0) - 16, 60, LV_TEXT_ALIGN_RIGHT, theme::ACCENT);
    text(layer, "- - beste", ox + GX1 - 56, py(oy, 0) - 16, 54, LV_TEXT_ALIGN_RIGHT, theme::GOOD);
  }

  lv_obj_t* graph_ = nullptr;
  lv_obj_t* timeRow_ = nullptr;
  lv_obj_t* time_ = nullptr;
  lv_obj_t* sub_ = nullptr;
  lv_obj_t* badge_ = nullptr;
  lv_obj_t* badgeText_ = nullptr;
  lv_obj_t* last_[3] = {};
  lv_obj_t* best_[3] = {};
  lv_obj_t* dLast_ = nullptr;
  lv_obj_t* dBest_ = nullptr;
  lv_obj_t* dDiff_ = nullptr;
  uint32_t detailGen_ = 0xFFFFFFFF;
  int detailIdx_ = 0;
  char dShown_[96] = "";
  int shownMode_ = -1;
  char shownBadge_[32] = "";
  char shownTime_[16] = "";
  char shownSub_[48] = "";
  uint32_t shownTimeColor_ = 0xFFFFFFFF;
  char shownLast_[3][16] = {};
  char shownBest_[3][20] = {};
  uint32_t drawnSig_ = 0xFFFFFFFF;
  uint32_t lastDraw_ = 0;
  CarSnapshot snap_;
};

}  // namespace

Page* sprintPage() {
  static SprintPage page;
  return &page;
}
