#include "theme.h"

lv_font_t font_small;
lv_font_t font_v24;
lv_font_t font_v32;
lv_font_t font_v40;

namespace theme {

namespace {
lv_style_t sText;
lv_style_t sMuted;
bool isNight = false;
}  // namespace

void init(lv_display_t* disp) {
  font_small = lv_font_montserrat_10;
  font_small.fallback = &font_m12;
  font_v24 = lv_font_montserrat_24;
  font_v24.fallback = &font_m20;
  font_v32 = lv_font_montserrat_32;
  font_v32.fallback = &font_m28;
  font_v40 = lv_font_montserrat_40;
  font_v40.fallback = &font_m28;
  lv_theme_t* th = lv_theme_default_init(disp, c(ACCENT), c(GOOD), true, &font_m14);
  lv_display_set_theme(disp, th);

  lv_style_init(&sText);
  lv_style_set_text_color(&sText, c(TEXT));
  lv_style_init(&sMuted);
  lv_style_set_text_color(&sMuted, c(MUTED));

  lv_obj_t* scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, c(BG), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(scr, c(TEXT), 0);
  lv_obj_set_style_text_font(scr, &font_m14, 0);
}

void setNight(bool night) {
  if (night == isNight) return;
  isNight = night;
  lv_style_set_text_color(&sText, c(night ? TEXT_NIGHT : TEXT));
  lv_obj_report_style_change(&sText);
  lv_obj_set_style_text_color(lv_screen_active(), c(night ? TEXT_NIGHT : TEXT), 0);
}

bool night() { return isNight; }

lv_style_t* textStyle() { return &sText; }
lv_style_t* mutedStyle() { return &sMuted; }

lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, bool muted, const char* text) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_add_style(l, muted ? &sMuted : &sText, 0);
  lv_obj_set_style_text_font(l, font, 0);
  lv_label_set_text(l, text);
  return l;
}

}  // namespace theme
