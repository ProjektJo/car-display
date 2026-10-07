// Seite "Übersicht": sechs frei belegbare Kacheln (U Seite 4).
// Tippen auf eine Kachel zeigt den Verlauf der letzten 5 min mit Min/Max, langes Drücken (0,8 s)
// wählt den Wert der Kachel. Die Belegung wird gespeichert (NVS). Reichweite mit geteiltem Balken.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "page.h"
#include "ui/overlay.h"
#include "ui/range_bar.h"
#include "ui/theme.h"
#include "ui/ui_prefs.h"
#include "ui/value_ui.h"
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

using values::Key;

Key keyOf(int tile) {
  Key k = Key::Speed;
  values::fromId(uiprefs::get().tiles[tile], k);
  return k;
}

// ---------- Kachel belegen (lang drücken) ----------
void onPicked(Key k, void* user) {
  const int tile = static_cast<int>(reinterpret_cast<intptr_t>(user));
  snprintf(uiprefs::get().tiles[tile], sizeof(uiprefs::get().tiles[tile]), "%s", values::id(k));
  uiprefs::save();
}

class OverviewPage : public Page {
 public:
  OverviewPage() : Page("Übersicht") {}

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

 private:
  static void onTileTap(lv_event_t* e) {
    const int i = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (overlay::isOpen()) return;
    valueui::openDetail(keyOf(i));
  }
  static void onTileLong(lv_event_t* e) {
    const int i = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (overlay::isOpen()) return;
    char title[40];
    snprintf(title, sizeof(title), "Kachel %d: Wert wählen", i + 1);
    valueui::openPicker(title, keyOf(i), onPicked, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
  }

  lv_obj_t* label_[TILES] = {};
  lv_obj_t* value_[TILES] = {};
  lv_obj_t* unit_[TILES] = {};
  RangeBar bar_[TILES];
  Key shownKey_[TILES];
  char shown_[TILES][16] = {};
  const char* shownUnit_[TILES] = {};
  uint32_t shownColor_[TILES] = {theme::TEXT, theme::TEXT, theme::TEXT, theme::TEXT, theme::TEXT, theme::TEXT};
};

}  // namespace

Page* overviewPage() {
  static OverviewPage page;
  return &page;
}
