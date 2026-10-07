// Untere Feldleiste (Eco, Sport, Sprint): 2–4 frei belegbare Felder mit Beschriftung oben und großem,
// mittigem Wert mit Einheit. Tippen zeigt den Wert groß (Detail), lang drücken (0,8 s) wählt den Wert.
// Die Belegung steht in UiSettings::fields[page] (NVS). Passt ein Wert nicht in die Breite, wird die
// Schrift kleiner und zuletzt die Einheit weggelassen.
#pragma once
#include <lvgl.h>

#include "core/car_state.h"
#include "core/ui_settings.h"
#include "ui/values.h"

class FieldBar {
 public:
  static constexpr int MAX = UiSettings::FIELDS;
  // page: 0 Eco, 1 Sport, 2 Sprint; y/h: Lage der Leiste auf der Seite
  void create(lv_obj_t* parent, int page, int count, int32_t y, int32_t h);
  void update(const CarSnapshot& s);
  lv_obj_t* field(int i) const { return tile_[i]; }

 private:
  values::Key keyOf(int i) const;
  void fit(int i, const char* text, const char* unit);
  static void onTap(lv_event_t* e);
  static void onLong(lv_event_t* e);
  static void onPicked(values::Key k, void* user);

  int page_ = 0;
  int count_ = 0;
  int32_t innerW_ = 0;
  lv_obj_t* tile_[MAX] = {};
  lv_obj_t* label_[MAX] = {};
  lv_obj_t* value_[MAX] = {};
  lv_obj_t* unit_[MAX] = {};
  values::Key shownKey_[MAX] = {values::Key::COUNT, values::Key::COUNT, values::Key::COUNT, values::Key::COUNT};
  char shown_[MAX][20] = {};
  const char* shownUnit_[MAX] = {};
  uint32_t shownColor_[MAX] = {};
  const lv_font_t* shownFont_[MAX] = {};
  bool shownUnitVis_[MAX] = {};
};
