#include "field_bar.h"

#include <cstdio>
#include <cstring>

#include "ui/overlay.h"
#include "ui/theme.h"
#include "ui/ui_prefs.h"
#include "ui/value_ui.h"
#include "util/format.h"

namespace {
constexpr int32_t SIDE = 8, GAP = 6;  // Rand und Abstand wie die Kacheln
constexpr int32_t PAD_HOR = 4, UNIT_GAP = 3;

struct Ctx {
  FieldBar* bar;
  int index;
};
Ctx ctx[UiSettings::FIELD_PAGES][FieldBar::MAX];
}  // namespace

values::Key FieldBar::keyOf(int i) const {
  values::Key k = values::Key::Speed;
  if (!values::fromId(uiprefs::get().fields[page_][i], k)) {
    // leer oder unbekannt: Standard der Seite
    const UiSettings def;
    values::fromId(def.fields[page_][i], k);
  }
  return k;
}

void FieldBar::create(lv_obj_t* parent, int page, int count, int32_t y, int32_t h) {
  page_ = page;
  count_ = count > MAX ? MAX : count;
  const int32_t w = (BOARD_LCD_HOR_RES - 2 * SIDE - (count_ - 1) * GAP) / count_;
  innerW_ = w - 2 * PAD_HOR;
  for (int i = 0; i < count_; i++) {
    lv_obj_t* t = lv_obj_create(parent);
    lv_obj_remove_style_all(t);
    lv_obj_set_size(t, w, h);
    lv_obj_set_pos(t, SIDE + i * (w + GAP), y);
    lv_obj_set_style_bg_color(t, theme::c(theme::SURFACE), 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(t, theme::c(theme::LINE), LV_STATE_PRESSED);
    lv_obj_set_style_radius(t, theme::RADIUS_TILE, 0);
    lv_obj_set_style_pad_hor(t, PAD_HOR, 0);
    lv_obj_set_style_pad_ver(t, 1, 0);
    lv_obj_set_flex_flow(t, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(t, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(t, 0, 0);
    lv_obj_add_flag(t, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(t, LV_OBJ_FLAG_SCROLLABLE);
    ctx[page][i] = Ctx{this, i};
    lv_obj_add_event_cb(t, onTap, LV_EVENT_SHORT_CLICKED, &ctx[page][i]);
    lv_obj_add_event_cb(t, onLong, LV_EVENT_LONG_PRESSED, &ctx[page][i]);
    tile_[i] = t;

    label_[i] = theme::label(t, &font_m12, true, "");
    lv_obj_t* row = lv_obj_create(t);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(row, UNIT_GAP, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    value_[i] = theme::label(row, &font_v24, false, fmt::NO_VALUE);
    unit_[i] = theme::label(row, &font_m12, true, "");
    lv_obj_set_style_pad_bottom(unit_[i], 4, 0);
    shownFont_[i] = &font_v24;
    shownUnitVis_[i] = true;
    shownColor_[i] = theme::TEXT;
  }
}

// Schrift und Einheit so wählen, dass der Wert in die Feldbreite passt
void FieldBar::fit(int i, const char* text, const char* unit) {
  auto width = [](const char* t, const lv_font_t* f) {
    lv_point_t p;
    lv_text_get_size(&p, t, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return p.x;
  };
  const int32_t uw = (unit && *unit) ? width(unit, &font_m12) + UNIT_GAP : 0;
  const lv_font_t* font = &font_v24;
  bool unitVis = true;
  if (width(text, font) + uw > innerW_) {
    font = &font_m20;
    if (width(text, font) + uw > innerW_) unitVis = false;
  }
  if (font != shownFont_[i]) {
    shownFont_[i] = font;
    lv_obj_set_style_text_font(value_[i], font, 0);
  }
  if (unitVis != shownUnitVis_[i]) {
    shownUnitVis_[i] = unitVis;
    if (unitVis) lv_obj_remove_flag(unit_[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(unit_[i], LV_OBJ_FLAG_HIDDEN);
  }
}

void FieldBar::update(const CarSnapshot& s) {
  for (int i = 0; i < count_; i++) {
    const values::Key k = keyOf(i);
    if (k != shownKey_[i]) {
      shownKey_[i] = k;
      lv_label_set_text(label_[i], values::label(k));
      shown_[i][0] = '\0';
      shownUnit_[i] = nullptr;
    }
    const char* unit = values::unit(k, s);
    char text[20];
    values::text(k, s, text, sizeof(text));
    const bool unitChanged = unit != shownUnit_[i];
    if (unitChanged) {
      shownUnit_[i] = unit;
      lv_label_set_text_static(unit_[i], unit);
    }
    if (unitChanged || strcmp(text, shown_[i]) != 0) {
      snprintf(shown_[i], sizeof(shown_[i]), "%s", text);
      lv_label_set_text(value_[i], text);
      fit(i, text, unit);
    }
    const uint32_t color = values::color(k, s);
    if (color != shownColor_[i]) {
      shownColor_[i] = color;
      lv_obj_set_style_text_color(value_[i], theme::c(color), 0);
    }
  }
}

void FieldBar::onTap(lv_event_t* e) {
  const Ctx* c = static_cast<const Ctx*>(lv_event_get_user_data(e));
  if (overlay::isOpen()) return;
  valueui::openDetail(c->bar->keyOf(c->index));
}

void FieldBar::onLong(lv_event_t* e) {
  Ctx* c = static_cast<Ctx*>(lv_event_get_user_data(e));
  if (overlay::isOpen()) return;
  char title[40];
  snprintf(title, sizeof(title), "Feld %d: Wert wählen", c->index + 1);
  valueui::openPicker(title, c->bar->keyOf(c->index), onPicked, c);
}

void FieldBar::onPicked(values::Key k, void* user) {
  const Ctx* c = static_cast<const Ctx*>(user);
  char(&f)[12] = uiprefs::get().fields[c->bar->page_][c->index];
  snprintf(f, sizeof(f), "%s", values::id(k));
  uiprefs::save();
}
