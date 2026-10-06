#include "numpad.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ui/overlay.h"
#include "ui/theme.h"
#include "util/format.h"

namespace numpad {

namespace {
// Maße wie das Ziffernfeld des Tank-Fensters (Vorschau): Tasten 48 × 44 ab x 150
constexpr int32_t KEY_X = 144, KEY_Y = 2, KEY_W = 48, KEY_H = 44, KEY_STEP_X = 52, KEY_STEP_Y = 48;
constexpr const char* KEY_BACK = "\xEF\x95\x9A";  // U+F55A Löschtaste

char entry[10] = "";
int maxLen = 6;
DoneFn doneFn = nullptr;
void (*backFn)() = nullptr;
lv_obj_t* entryLbl = nullptr;
char placeholder[16] = "";

void show() {
  if (entry[0]) {
    // Tausenderpunkt wie überall (15.000)
    char t[16];
    fmt::number(t, sizeof(t), static_cast<float>(atof(entry)), 0);
    lv_label_set_text(entryLbl, t);
    lv_obj_set_style_text_color(entryLbl, theme::c(theme::TEXT), 0);
  } else {
    lv_label_set_text(entryLbl, placeholder);
    lv_obj_set_style_text_color(entryLbl, theme::c(theme::MUTED), 0);
  }
}

void onKey(lv_event_t* e) {
  const char* k = static_cast<const char*>(lv_event_get_user_data(e));
  size_t n = strlen(entry);
  if (strcmp(k, KEY_BACK) == 0) {
    if (n) entry[n - 1] = '\0';
  } else if (static_cast<int>(n) < maxLen && !(n == 0 && *k == '0')) {
    entry[n] = *k;
    entry[n + 1] = '\0';
  }
  show();
}

void finish() {
  void (*b)() = backFn;
  backFn = nullptr;
  overlay::close();
  if (b) b();
}

void onTake(lv_event_t*) {
  if (entry[0] && doneFn) doneFn(static_cast<float>(atof(entry)));
  finish();
}

void onCancel(lv_event_t*) { finish(); }

lv_obj_t* key(lv_obj_t* card, int32_t x, int32_t y, int32_t w, int32_t h, const char* text, lv_event_cb_t cb, void* user,
              bool accent = false) {
  lv_obj_t* b = lv_obj_create(card);
  lv_obj_remove_style_all(b);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(b, theme::RADIUS_TILE, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, theme::c(accent ? theme::ACCENT : theme::LINE), 0);
  lv_obj_set_style_bg_color(b, theme::c(accent ? theme::ACCENT : theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, theme::c(theme::LINE), LV_STATE_PRESSED);
  lv_obj_t* l = theme::label(b, accent ? &font_m14 : &font_m20, false, text);
  if (accent) lv_obj_set_style_text_color(l, theme::c(theme::BG), 0);
  lv_obj_center(l);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
  return b;
}
}  // namespace

void open(const char* title, const char* unit, float initial, int maxDigits, DoneFn done, void (*back)()) {
  entry[0] = '\0';
  maxLen = maxDigits < static_cast<int>(sizeof(entry)) - 1 ? maxDigits : static_cast<int>(sizeof(entry)) - 1;
  doneFn = done;
  backFn = back;
  fmt::number(placeholder, sizeof(placeholder), initial, 0);
  lv_obj_t* card = overlay::open("", true, theme::DIALOG_INSET_SUB);
  lv_obj_set_layout(card, LV_LAYOUT_NONE);
  lv_obj_t* t = theme::label(card, &font_m14, false, title);
  lv_obj_set_pos(t, 0, 0);
  lv_obj_set_width(t, KEY_X - 8);
  lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
  entryLbl = theme::label(card, &font_m28, false, "");
  lv_obj_set_pos(entryLbl, 0, 44);
  lv_obj_t* u = theme::label(card, &font_m12, true, unit);
  lv_obj_set_pos(u, 0, 80);
  show();
  static const char* const KEYS[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "", "0", KEY_BACK};
  for (int i = 0; i < 12; i++) {
    if (!KEYS[i][0]) continue;
    key(card, KEY_X + (i % 3) * KEY_STEP_X - 12, KEY_Y + (i / 3) * KEY_STEP_Y, KEY_W, KEY_H, KEYS[i], onKey,
        const_cast<char*>(KEYS[i]));
  }
  key(card, 0, 118, 120, 34, "Übernehmen", onTake, nullptr, true);
  lv_obj_t* c = key(card, 0, 158, 120, 30, "Abbrechen", onCancel, nullptr);
  lv_obj_set_style_text_font(lv_obj_get_child(c, 0), &font_m12, 0);
  lv_obj_set_style_text_color(lv_obj_get_child(c, 0), theme::c(theme::MUTED), 0);
}

}  // namespace numpad
