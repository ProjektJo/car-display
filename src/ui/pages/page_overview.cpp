// Seite "Übersicht": sechs frei belegbare Kacheln (U Seite 4).
// Etappe 3: vorläufig fest mit der Standardbelegung aus dem UI-Entwurf (Tempo, Drehzahl, Momentan,
// Ø 10 km, Gaspedal, Reichweite), damit die Rechnung am Gerät prüfbar ist. Belegbare Kacheln und
// Detailverlauf kommen in Etappe 5.
#include <cmath>
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

// Wert, Einheit und Nachkommastellen einer Kachel aus dem Snapshot
struct TileValue {
  float value;
  const char* unit;
  int decimals;
};
using TileFn = TileValue (*)(const CarSnapshot& s);

struct TileDef {
  const char* label;
  TileFn fn;
};

const TileDef DEFS[TILES] = {
    {"Tempo", [](const CarSnapshot& s) { return TileValue{s.speed.get(s.now), "km/h", 0}; }},
    {"Drehzahl", [](const CarSnapshot& s) { return TileValue{s.rpm.get(s.now), "U/min", 0}; }},
    // Momentan: l/100 km ab 5 km/h, darunter l/h (A7)
    {"Momentan", [](const CarSnapshot& s) {
       const float l100 = s.fuelL100.get(s.now);
       if (!std::isnan(l100)) return TileValue{l100, "l/100 km", 1};
       return TileValue{s.fuelLph.get(s.now), "l/h", 1};
     }},
    {SYM_AVG " 10 km", [](const CarSnapshot& s) { return TileValue{s.avg10.get(s.now), "l/100 km", 1}; }},
    // ANNAHME: ohne Gaspedal (0x49) zeigt die Kachel die Drosselklappe (0x11), wie der Scheduler (A7)
    {"Gaspedal", [](const CarSnapshot& s) {
       const float p = s.pedal.get(s.now);
       return TileValue{std::isnan(p) ? s.throttle.get(s.now) : p, "%", 0};
     }},
    {"Reichweite", [](const CarSnapshot& s) { return TileValue{s.rangeKm.get(s.now), "km", 0}; }},
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
      unit_[i] = theme::label(tile, &font_m12, true, "");
      lv_obj_set_pos(unit_[i], 0, UNIT_TOP);
      shown_[i][0] = '\0';
      shownUnit_[i] = nullptr;
    }
  }

  void update(const CarSnapshot& s) override {
    for (int i = 0; i < TILES; i++) {
      const TileValue v = DEFS[i].fn(s);
      if (v.unit != shownUnit_[i]) {
        shownUnit_[i] = v.unit;
        lv_label_set_text_static(unit_[i], v.unit);
      }
      char text[16];
      fmt::number(text, sizeof(text), v.value, v.decimals);
      if (strcmp(text, shown_[i]) != 0) {
        snprintf(shown_[i], sizeof(shown_[i]), "%s", text);
        lv_label_set_text(value_[i], text);
      }
    }
  }

 private:
  lv_obj_t* value_[TILES] = {};
  lv_obj_t* unit_[TILES] = {};
  char shown_[TILES][16] = {};
  const char* shownUnit_[TILES] = {};
};

}  // namespace

Page* overviewPage() {
  static OverviewPage page;
  return &page;
}
