// Seite "Übersicht": sechs frei belegbare Kacheln (U Seite 4).
// Etappe 1: vorläufig sechs feste Kacheln mit Rohwerten, damit die Datenquelle (Simulator,
// CarState, Snapshot) und das "–" für veraltete Werte prüfbar sind. Belegbare Kacheln,
// Detailverlauf und die Standardbelegung (Tempo, Drehzahl, Momentan, Ø 10 km, Gaspedal,
// Reichweite) kommen in Etappe 5.
#include <cstdio>
#include <cstring>

#include "page.h"
#include "ui/symbols.h"
#include "ui/theme.h"
#include "util/format.h"

namespace {

// Raster aus der Vorschau: 3 x 2 Kacheln, 100 x 102 px, Abstand 4 px, Rand 6 px
constexpr int COLS = 3;
constexpr int TILES = 6;
constexpr int32_t TILE_W = 100, TILE_H = 102;
constexpr int32_t ORIGIN = 6, STEP_X = 104, STEP_Y = 106;
constexpr int32_t TILE_PAD_HOR = 8, TILE_PAD_VER = 6;
constexpr int32_t VALUE_TOP = 22;    // Wert unter der Beschriftung
constexpr int32_t UNIT_TOP = 58;     // Einheit unter dem Wert

struct TileDef {
  const char* label;
  const char* unit;
  int decimals;
  Val CarState::*value;
};
const TileDef DEFS[TILES] = {
    {"Tempo", "km/h", 0, &CarState::speed},
    {"Drehzahl", "U/min", 0, &CarState::rpm},
    {"Gaspedal", "%", 0, &CarState::pedal},
    {"Kühlmittel", SYM_DEG "C", 0, &CarState::coolant},
    {"Saugrohr", "kPa", 0, &CarState::map},
    {"Spannung", "V", 1, &CarState::voltage},
};

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
      lv_obj_set_style_radius(tile, theme::RADIUS_TILE, 0);
      lv_obj_set_style_pad_hor(tile, TILE_PAD_HOR, 0);
      lv_obj_set_style_pad_ver(tile, TILE_PAD_VER, 0);
      lv_obj_remove_flag(tile, LV_OBJ_FLAG_CLICKABLE);  // Etappe 1: Druck geht an die Seite (lang = Menü)
      lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

      theme::label(tile, &font_m12, true, DEFS[i].label);
      value_[i] = theme::label(tile, &font_m28, false, fmt::NO_VALUE);
      lv_obj_set_pos(value_[i], 0, VALUE_TOP);
      lv_obj_t* unit = theme::label(tile, &font_m12, true, DEFS[i].unit);
      lv_obj_set_pos(unit, 0, UNIT_TOP);
      shown_[i][0] = '\0';
    }
  }

  void update(const CarSnapshot& s) override {
    for (int i = 0; i < TILES; i++) {
      char text[16];
      fmt::number(text, sizeof(text), (s.*DEFS[i].value).get(s.now), DEFS[i].decimals);
      if (strcmp(text, shown_[i]) != 0) {
        snprintf(shown_[i], sizeof(shown_[i]), "%s", text);
        lv_label_set_text(value_[i], text);
      }
    }
  }

 private:
  lv_obj_t* value_[TILES] = {};
  char shown_[TILES][16] = {};
};

}  // namespace

Page* overviewPage() {
  static OverviewPage page;
  return &page;
}
