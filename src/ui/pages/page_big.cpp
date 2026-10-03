// Seite "Großanzeige": ein Wert in 72 px, mit Pfeilen durchschaltbar (U Seite 5).
// Etappe 1: vorläufig nur das Tempo, um die 72-px-Ziffernschrift zu prüfen.
// Durchschalten der Werte kommt in Etappe 5.
#include <cstdio>
#include <cstring>

#include "page.h"
#include "ui/theme.h"
#include "util/format.h"

namespace {

// Positionen aus der Vorschau (pGross): Block ab 28 px, Beschriftung, Wert, Einheit
constexpr int32_t BLOCK_TOP = 28;

class BigPage : public Page {
 public:
  BigPage() : Page("Großanzeige") {}

  void create(lv_obj_t* parent) override {
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_pos(box, 0, BLOCK_TOP);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
    theme::label(box, &font_m14, true, "Tempo");
    value_ = theme::label(box, &font_d72, false, fmt::NO_VALUE);
    theme::label(box, &font_m14, true, "km/h");
  }

  void update(const CarSnapshot& s) override {
    char text[16];
    fmt::number(text, sizeof(text), s.speed.get(s.now), 0);
    if (strcmp(text, shown_) == 0) return;
    snprintf(shown_, sizeof(shown_), "%s", text);
    lv_label_set_text(value_, text);
  }

 private:
  lv_obj_t* value_ = nullptr;
  char shown_[16] = "";
};

}  // namespace

Page* bigPage() {
  static BigPage page;
  return &page;
}
