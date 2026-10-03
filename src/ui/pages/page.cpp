#include "page.h"

#include <cstdio>

#include "ui/theme.h"

void PlaceholderPage::create(lv_obj_t* parent) {
  lv_obj_t* box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_center(box);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
  theme::label(box, &font_m20, false, name());
  char text[40];
  snprintf(text, sizeof(text), "folgt in Etappe %d", stage_);
  theme::label(box, &font_m12, true, text);
}
