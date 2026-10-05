#include "range_bar.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "ui/symbols.h"
#include "ui/theme.h"
#include "util/format.h"

namespace {
constexpr int32_t BAR_H = 5;
constexpr int32_t TEXT_GAP = 3;
constexpr lv_opa_t DRIVEN_OPA = 140;  // Vorschau: muted mit Deckkraft 0,55

void setText(lv_obj_t* l, char* shown, size_t size, const char* t) {
  if (strcmp(shown, t) == 0) return;
  snprintf(shown, size, "%s", t);
  lv_label_set_text(l, t);
}
}  // namespace

void RangeBar::create(lv_obj_t* parent, int32_t width, bool bigText) {
  width_ = width;
  root_ = lv_obj_create(parent);
  lv_obj_remove_style_all(root_);
  lv_obj_set_size(root_, width, LV_SIZE_CONTENT);
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* bar = lv_obj_create(root_);
  lv_obj_remove_style_all(bar);
  lv_obj_set_size(bar, width, BAR_H);
  lv_obj_set_style_radius(bar, 3, 0);
  lv_obj_set_style_clip_corner(bar, true, 0);
  lv_obj_set_style_bg_color(bar, theme::c(theme::BG), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
  driven_ = lv_obj_create(bar);
  lv_obj_remove_style_all(driven_);
  lv_obj_set_size(driven_, 0, BAR_H);
  lv_obj_set_style_bg_color(driven_, theme::c(theme::MUTED), 0);
  lv_obj_set_style_bg_opa(driven_, DRIVEN_OPA, 0);
  rest_ = lv_obj_create(bar);
  lv_obj_remove_style_all(rest_);
  lv_obj_set_size(rest_, width, BAR_H);
  lv_obj_set_style_bg_color(rest_, theme::c(theme::ACCENT), 0);
  lv_obj_set_style_bg_opa(rest_, LV_OPA_COVER, 0);
  shownColor_ = theme::ACCENT;

  const lv_font_t* f = bigText ? &font_m14 : &font_small;
  left_ = theme::label(root_, f, true, "");
  lv_obj_set_pos(left_, 0, BAR_H + TEXT_GAP);
  right_ = theme::label(root_, f, true, "");
  lv_obj_align(right_, LV_ALIGN_TOP_RIGHT, 0, BAR_H + TEXT_GAP);
}

void RangeBar::update(const CarSnapshot& s) {
  const float drv = s.fillKm.get(s.now);
  const float rem = s.rangeKm.get(s.now);
  // Balken: Anteil gefahren an der ganzen Tankfüllung
  int32_t dw = 0;
  if (!std::isnan(drv) && !std::isnan(rem) && drv + rem > 0) dw = static_cast<int32_t>(std::lround(drv / (drv + rem) * width_));
  else if (std::isnan(rem)) dw = width_;  // ohne Reichweite nur grau
  if (dw < 0) dw = 0;
  if (dw > width_) dw = width_;
  if (dw != shownDrivenW_) {
    shownDrivenW_ = dw;
    lv_obj_set_width(driven_, dw);
    lv_obj_set_x(rest_, dw);
    lv_obj_set_width(rest_, width_ - dw);
  }
  const uint32_t c = (!std::isnan(rem) && rem < cfg::RANGE_LOW_KM) ? theme::WARN : theme::ACCENT;
  if (c != shownColor_) {
    shownColor_ = c;
    lv_obj_set_style_bg_color(rest_, theme::c(c), 0);
  }
  char n[16], t[24];
  fmt::number(n, sizeof(n), drv, 0);
  snprintf(t, sizeof(t), SYM_PUMP " " SYM_DOT SYM_DOT SYM_DOT " %s", n);
  setText(left_, shownLeft_, sizeof(shownLeft_), t);
  fmt::number(n, sizeof(n), (std::isnan(drv) || std::isnan(rem)) ? NAN : drv + rem, 0);
  snprintf(t, sizeof(t), SYM_SIGMA " %s km", n);
  setText(right_, shownRight_, sizeof(shownRight_), t);
}
