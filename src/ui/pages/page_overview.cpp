// Seite "Übersicht": sechs frei belegbare Kacheln (U Seite 4).
// Tippen auf eine Kachel zeigt den Verlauf der letzten 5 min mit Min/Max, langes Drücken (0,8 s)
// wählt den Wert der Kachel. Die Belegung wird gespeichert (NVS). Reichweite mit geteiltem Balken.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "page.h"
#include "ui/history.h"
#include "ui/linechart.h"
#include "ui/overlay.h"
#include "ui/range_bar.h"
#include "ui/theme.h"
#include "ui/ui_prefs.h"
#include "ui/values.h"
#include "util/format.h"

namespace {

// Raster aus der Vorschau: 3 x 2 Kacheln, 100 x 102 px, Abstand 4 px, Rand 6 px
constexpr int COLS = 3;
constexpr int TILES = UiSettings::TILES;
constexpr int32_t TILE_W = 100, TILE_H = 102;
constexpr int32_t ORIGIN = 6, STEP_X = 104, STEP_Y = 106;
constexpr int32_t TILE_PAD_HOR = 8, TILE_PAD_VER = 6;
constexpr int32_t VALUE_TOP = 22;    // Wert unter der Beschriftung
constexpr int32_t UNIT_TOP = 58;     // Einheit unter dem Wert
constexpr int32_t BAR_BOTTOM = 7;    // Reichweitenbalken 7 px über dem unteren Rand
constexpr int32_t BAR_W = 84;
// Detail: Diagramm der letzten 5 min (Vorschau: 270 × 100 ab 12/70, Zeichenfläche 22/6, 244 × 84)
constexpr int DETAIL_WINDOW_S = 300;
constexpr int32_t DETAIL_CHART_Y = 56, DETAIL_CHART_H = 100;

using values::Key;

Key keyOf(int tile) {
  Key k = Key::Speed;
  values::fromId(uiprefs::get().tiles[tile], k);
  return k;
}

class OverviewPage;
OverviewPage* self = nullptr;

// ---------- Detail (Tippen) ----------
struct Detail {
  Key key = Key::Speed;
  uint32_t gen = 0;
  lv_obj_t* value = nullptr;
  lv_obj_t* unit = nullptr;
  lv_obj_t* chart = nullptr;
  lv_obj_t* minmax = nullptr;
  uint32_t drawnSeq = 0;
  char shown[24] = "";
} detail;

void onDetailClose(lv_event_t*) { overlay::close(); }

void detailDraw(lv_event_t* e) {
  linechart::Spec sp;
  sp.series[0] = values::series(detail.key);
  sp.windowS = DETAIL_WINDOW_S;
  sp.x = 30;
  sp.y = 8;
  sp.w = 236;
  sp.h = DETAIL_CHART_H - 16;
  linechart::draw(lv_event_get_layer(e), lv_event_get_target_obj(e), sp);
}

void openDetail(Key k, const CarSnapshot& s) {
  lv_obj_t* card = overlay::open("");
  detail.key = k;
  detail.gen = overlay::generation();
  detail.shown[0] = '\0';
  detail.drawnSeq = 0;
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(card, onDetailClose, LV_EVENT_CLICKED, nullptr);  // Tippen schließt
  lv_obj_add_event_cb(lv_obj_get_parent(card), onDetailClose, LV_EVENT_CLICKED, nullptr);
  lv_obj_set_layout(card, LV_LAYOUT_NONE);
  char title[48];
  snprintf(title, sizeof(title), "%s", values::label(k));
  lv_obj_t* t = theme::label(card, &font_m14, false, title);
  lv_obj_set_pos(t, 0, 0);
  const bool hasSeries = values::series(k) != values::Series::None;
  if (hasSeries) {
    lv_obj_t* sub = theme::label(card, &font_m12, true, "letzte 5 min");
    lv_obj_align_to(sub, t, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, 0);
  }
  lv_obj_t* row = lv_obj_create(card);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_pos(row, 0, 20);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_column(row, 4, 0);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
  detail.value = theme::label(row, &font_m28, false, "");
  detail.unit = theme::label(row, &font_m12, true, "");
  lv_obj_set_style_pad_bottom(detail.unit, 5, 0);
  if (hasSeries) {
    detail.chart = lv_obj_create(card);
    lv_obj_remove_style_all(detail.chart);
    lv_obj_set_pos(detail.chart, 0, DETAIL_CHART_Y);
    lv_obj_set_size(detail.chart, LV_PCT(100), DETAIL_CHART_H);
    lv_obj_remove_flag(detail.chart, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(detail.chart, detailDraw, LV_EVENT_DRAW_MAIN, nullptr);
    detail.minmax = theme::label(card, &font_m12, true, "");
    lv_obj_align(detail.minmax, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  } else {
    detail.chart = nullptr;
    detail.minmax = nullptr;
    lv_obj_t* note = theme::label(card, &font_m12, true,
                                  "Dieser Wert wird über die Strecke gemittelt, nicht über die Zeit. Tippen schließt.");
    lv_obj_set_width(note, LV_PCT(100));
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(note, 0, 70);
  }
  (void)s;
}

void updateDetail(const CarSnapshot& s) {
  if (!overlay::isOpen() || overlay::generation() != detail.gen) return;
  char t[24];
  values::text(detail.key, s, t, sizeof(t));
  if (strcmp(t, detail.shown) != 0) {
    snprintf(detail.shown, sizeof(detail.shown), "%s", t);
    lv_label_set_text(detail.value, t);
    lv_obj_set_style_text_color(detail.value, theme::c(values::color(detail.key, s)), 0);
    lv_label_set_text(detail.unit, values::unit(detail.key, s));
  }
  if (detail.chart && history::seq() != detail.drawnSeq) {
    detail.drawnSeq = history::seq();
    lv_obj_invalidate(detail.chart);
    float lo = 0, hi = 0;
    if (linechart::minMax(values::series(detail.key), DETAIL_WINDOW_S, lo, hi)) {
      char a[16], b[16], m[64];
      const int d = values::decimals(detail.key);
      fmt::number(a, sizeof(a), lo, d);
      fmt::number(b, sizeof(b), hi, d);
      snprintf(m, sizeof(m), "Min %s " "\xC2\xB7" " Max %s " "\xC2\xB7" " Tippen schließt", a, b);
      lv_label_set_text(detail.minmax, m);
    }
  }
}

// ---------- Kachel belegen (lang drücken) ----------
int pickTile = 0;

void onPick(lv_event_t* e) {
  const int k = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  snprintf(uiprefs::get().tiles[pickTile], sizeof(uiprefs::get().tiles[pickTile]), "%s", values::id(static_cast<Key>(k)));
  uiprefs::save();
  overlay::close();
}

void openPick(int tile) {
  pickTile = tile;
  char title[40];
  snprintf(title, sizeof(title), "Kachel %d: Wert wählen", tile + 1);
  lv_obj_t* card = overlay::open(title);
  lv_obj_t* grid = lv_obj_create(card);
  lv_obj_remove_style_all(grid);
  lv_obj_set_size(grid, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_column(grid, 8, 0);
  lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_update_layout(card);
  const int32_t colW = (lv_obj_get_content_width(card) - 8) / 2;
  const Key current = keyOf(tile);
  for (int i = 0; i < values::COUNT; i++) {
    lv_obj_t* r = lv_obj_create(grid);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, colW, 20);
    lv_obj_set_style_pad_hor(r, 2, 0);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_set_style_border_color(r, theme::c(theme::LINE), 0);
    lv_obj_set_style_bg_color(r, theme::c(theme::LINE), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(r, onPick, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
    lv_obj_t* l = theme::label(r, &font_m12, false, values::label(static_cast<Key>(i)));
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    if (static_cast<Key>(i) == current) lv_obj_set_style_text_color(l, theme::c(theme::ACCENT), 0);
  }
}

class OverviewPage : public Page {
 public:
  OverviewPage() : Page("Übersicht") { self = this; }

  void create(lv_obj_t* parent) override {
    for (int i = 0; i < TILES; i++) {
      lv_obj_t* tile = lv_obj_create(parent);
      lv_obj_remove_style_all(tile);
      lv_obj_set_pos(tile, ORIGIN + (i % COLS) * STEP_X, ORIGIN + (i / COLS) * STEP_Y);
      lv_obj_set_size(tile, TILE_W, TILE_H);
      lv_obj_set_style_bg_color(tile, theme::c(theme::SURFACE), 0);
      lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
      lv_obj_set_style_bg_color(tile, theme::c(theme::LINE), LV_STATE_PRESSED);
      lv_obj_set_style_radius(tile, theme::RADIUS_TILE, 0);
      lv_obj_set_style_pad_hor(tile, TILE_PAD_HOR, 0);
      lv_obj_set_style_pad_ver(tile, TILE_PAD_VER, 0);
      lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_event_cb(tile, onTileTap, LV_EVENT_SHORT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
      lv_obj_add_event_cb(tile, onTileLong, LV_EVENT_LONG_PRESSED, reinterpret_cast<void*>(static_cast<intptr_t>(i)));

      label_[i] = theme::label(tile, &font_m12, true, "");
      value_[i] = theme::label(tile, &font_m28, false, fmt::NO_VALUE);
      lv_obj_set_pos(value_[i], 0, VALUE_TOP);
      unit_[i] = theme::label(tile, &font_m12, true, "");
      lv_obj_set_pos(unit_[i], 0, UNIT_TOP);
      bar_[i].create(tile, BAR_W);
      lv_obj_align(bar_[i].obj(), LV_ALIGN_BOTTOM_LEFT, 0, -(BAR_BOTTOM - TILE_PAD_VER));
      lv_obj_add_flag(bar_[i].obj(), LV_OBJ_FLAG_HIDDEN);
      shownKey_[i] = Key::COUNT;
    }
  }

  void update(const CarSnapshot& s) override {
    last_ = s;
    for (int i = 0; i < TILES; i++) {
      const Key k = keyOf(i);
      if (k != shownKey_[i]) {
        shownKey_[i] = k;
        lv_label_set_text(label_[i], values::label(k));
        shown_[i][0] = '\0';
        shownUnit_[i] = nullptr;
        // Reichweite: statt der Einheit der Balken mit "Σ … km" (U Seite 4)
        if (k == Key::Range) {
          lv_obj_remove_flag(bar_[i].obj(), LV_OBJ_FLAG_HIDDEN);
          lv_obj_add_flag(unit_[i], LV_OBJ_FLAG_HIDDEN);
        } else {
          lv_obj_add_flag(bar_[i].obj(), LV_OBJ_FLAG_HIDDEN);
          lv_obj_remove_flag(unit_[i], LV_OBJ_FLAG_HIDDEN);
        }
      }
      const char* unit = values::unit(k, s);
      if (unit != shownUnit_[i]) {
        shownUnit_[i] = unit;
        lv_label_set_text_static(unit_[i], unit);
      }
      const uint32_t color = values::color(k, s);
      if (color != shownColor_[i]) {
        shownColor_[i] = color;
        lv_obj_set_style_text_color(value_[i], theme::c(color), 0);
      }
      char text[16];
      values::text(k, s, text, sizeof(text));
      if (strcmp(text, shown_[i]) != 0) {
        snprintf(shown_[i], sizeof(shown_[i]), "%s", text);
        lv_label_set_text(value_[i], text);
      }
      if (k == Key::Range) bar_[i].update(s);
    }
  }

  // Detail läuft weiter, auch wenn die Seite gewechselt wurde (das Fenster liegt darüber)
  void tick(const CarSnapshot& s) override { updateDetail(s); }

 private:
  static void onTileTap(lv_event_t* e) {
    const int i = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (overlay::isOpen()) return;
    openDetail(keyOf(i), self->last_);
  }
  static void onTileLong(lv_event_t* e) {
    const int i = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (overlay::isOpen()) return;
    openPick(i);
  }

  lv_obj_t* label_[TILES] = {};
  lv_obj_t* value_[TILES] = {};
  lv_obj_t* unit_[TILES] = {};
  RangeBar bar_[TILES];
  Key shownKey_[TILES];
  char shown_[TILES][16] = {};
  const char* shownUnit_[TILES] = {};
  uint32_t shownColor_[TILES] = {theme::TEXT, theme::TEXT, theme::TEXT, theme::TEXT, theme::TEXT, theme::TEXT};
  CarSnapshot last_;
};

}  // namespace

Page* overviewPage() {
  static OverviewPage page;
  return &page;
}
