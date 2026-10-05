// Seite "Großanzeige": ein Wert in 72 px, Beschriftung darüber, links/rechts Pfeile zum Durchschalten
// (U Seite 5). Gedacht für Mitfahrer und den schnellen Blick. Die Wahl wird gespeichert.
#include <cstdio>
#include <cstring>

#include "page.h"
#include "ui/range_bar.h"
#include "ui/theme.h"
#include "ui/ui_prefs.h"
#include "ui/values.h"
#include "util/format.h"

namespace {

using values::Key;

// Werte der Großanzeige (Vorschau BIG)
constexpr Key BIG[] = {Key::Speed, Key::Inst, Key::Avg10, Key::Rpm, Key::Range, Key::Coolant};
constexpr int BIG_COUNT = sizeof(BIG) / sizeof(BIG[0]);

// Positionen aus der Vorschau (pGross)
constexpr int32_t SIDE_W = 60;        // Tippfläche für ‹ und ›
constexpr int32_t BLOCK_TOP = 28;     // Block ab 28 px, bei Reichweite ab 8 px (Platz für den Balken)
constexpr int32_t BLOCK_TOP_RANGE = 8;
constexpr int32_t BAR_W = 190;
constexpr int32_t DOT = 5, DOT_GAP = 4, DOTS_BOTTOM = 10;

class BigPage : public Page {
 public:
  BigPage() : Page("Großanzeige") {}

  void create(lv_obj_t* parent) override {
    for (int side = 0; side < 2; side++) {
      lv_obj_t* z = lv_obj_create(parent);
      lv_obj_remove_style_all(z);
      lv_obj_set_size(z, SIDE_W, theme::CONTENT_H);
      lv_obj_align(z, side ? LV_ALIGN_TOP_RIGHT : LV_ALIGN_TOP_LEFT, 0, 0);
      lv_obj_add_flag(z, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(z, onArrow, LV_EVENT_SHORT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(side ? 1 : -1)));
      lv_obj_t* a = theme::label(z, &font_m14, true, side ? "\xEF\x81\x94" : "\xEF\x81\x93");  // Pfeile (U+F054, U+F053)
      lv_obj_align(a, side ? LV_ALIGN_RIGHT_MID : LV_ALIGN_LEFT_MID, side ? -10 : 10, 0);
    }
    box_ = lv_obj_create(parent);
    lv_obj_remove_style_all(box_);
    lv_obj_set_size(box_, BOARD_LCD_HOR_RES - 2 * SIDE_W, LV_SIZE_CONTENT);
    lv_obj_set_pos(box_, SIDE_W, BLOCK_TOP);
    lv_obj_set_flex_flow(box_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(box_, LV_OBJ_FLAG_CLICKABLE);
    label_ = theme::label(box_, &font_m14, true, "");
    value_ = theme::label(box_, &font_d72, false, fmt::NO_VALUE);
    unit_ = theme::label(box_, &font_m14, true, "");
    bar_.create(box_, BAR_W, true);
    lv_obj_set_style_margin_top(bar_.obj(), 8, 0);

    lv_obj_t* dots = lv_obj_create(parent);
    lv_obj_remove_style_all(dots);
    lv_obj_set_size(dots, LV_SIZE_CONTENT, DOT);
    lv_obj_align(dots, LV_ALIGN_BOTTOM_MID, 0, -DOTS_BOTTOM);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(dots, DOT_GAP, 0);
    lv_obj_remove_flag(dots, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < BIG_COUNT; i++) {
      dot_[i] = lv_obj_create(dots);
      lv_obj_remove_style_all(dot_[i]);
      lv_obj_set_size(dot_[i], DOT, DOT);
      lv_obj_set_style_radius(dot_[i], LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_opa(dot_[i], LV_OPA_COVER, 0);
      lv_obj_set_style_bg_color(dot_[i], theme::c(theme::LINE), 0);
    }
  }

  void update(const CarSnapshot& s) override {
    const int idx = index();
    if (idx != shownIdx_) {
      if (shownIdx_ >= 0) lv_obj_set_style_bg_color(dot_[shownIdx_], theme::c(theme::LINE), 0);
      lv_obj_set_style_bg_color(dot_[idx], theme::c(theme::ACCENT), 0);
      shownIdx_ = idx;
      const Key k = BIG[idx];
      lv_label_set_text(label_, values::label(k));
      lv_obj_set_y(box_, k == Key::Range ? BLOCK_TOP_RANGE : BLOCK_TOP);
      if (k == Key::Range) lv_obj_remove_flag(bar_.obj(), LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(bar_.obj(), LV_OBJ_FLAG_HIDDEN);
      shown_[0] = '\0';
      shownUnit_ = nullptr;
    }
    const Key k = BIG[idx];
    char t[16];
    values::text(k, s, t, sizeof(t));
    if (strcmp(t, shown_) != 0) {
      snprintf(shown_, sizeof(shown_), "%s", t);
      lv_label_set_text(value_, t);
    }
    const uint32_t c = values::color(k, s);
    if (c != shownColor_) {
      shownColor_ = c;
      lv_obj_set_style_text_color(value_, theme::c(c), 0);
    }
    const char* u = values::unit(k, s);
    if (u != shownUnit_) {
      shownUnit_ = u;
      lv_label_set_text_static(unit_, u);
    }
    if (k == Key::Range) bar_.update(s);
  }

 private:
  static int index() {
    const int i = uiprefs::get().big;
    return i >= 0 && i < BIG_COUNT ? i : 0;
  }

  static void onArrow(lv_event_t* e) {
    const int dir = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    uiprefs::get().big = static_cast<uint8_t>((index() + dir + BIG_COUNT) % BIG_COUNT);
    uiprefs::save();
  }

  lv_obj_t* box_ = nullptr;
  lv_obj_t* label_ = nullptr;
  lv_obj_t* value_ = nullptr;
  lv_obj_t* unit_ = nullptr;
  lv_obj_t* dot_[BIG_COUNT] = {};
  RangeBar bar_;
  int shownIdx_ = -1;
  char shown_[16] = "";
  const char* shownUnit_ = nullptr;
  uint32_t shownColor_ = 0xFFFFFFFF;
};


}  // namespace

Page* bigPage() {
  static BigPage page;
  return &page;
}
