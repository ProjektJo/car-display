#include "overlay.h"

#include "config.h"
#include "ui/theme.h"

namespace overlay {

namespace {
// Maße aus der Vorschau (.dlg)
constexpr int32_t PAD_HOR = 12;
constexpr int32_t PAD_VER = 10;
constexpr int32_t TITLE_GAP = 6;

constexpr int32_t DONE_W = 54;
constexpr int32_t DONE_H = 22;
constexpr int32_t DONE_RIGHT = 8;
constexpr int32_t DONE_TOP = 6;

lv_obj_t* dim = nullptr;
bool autoCloseOn = true;
uint32_t openedMs = 0;
uint32_t gen = 0;
void (*onClose)() = nullptr;
}  // namespace

lv_obj_t* open(const char* title, bool autoClose, int32_t inset) {
  close();
  gen++;
  autoCloseOn = autoClose;
  openedMs = lv_tick_get();

  dim = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(dim);
  lv_obj_set_size(dim, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(dim, theme::c(theme::OVERLAY_DIM), 0);
  lv_obj_set_style_bg_opa(dim, theme::OVERLAY_DIM_OPA, 0);
  lv_obj_add_flag(dim, LV_OBJ_FLAG_CLICKABLE);  // Berührungen erreichen die Seite darunter nicht
  lv_obj_remove_flag(dim, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* card = lv_obj_create(dim);
  lv_obj_remove_style_all(card);
  lv_obj_set_pos(card, inset, inset);
  lv_obj_set_size(card, BOARD_LCD_HOR_RES - 2 * inset, BOARD_LCD_VER_RES - 2 * inset);
  lv_obj_set_style_bg_color(card, theme::c(theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(card, theme::c(theme::LINE), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_radius(card, theme::RADIUS_DIALOG, 0);
  lv_obj_set_style_pad_hor(card, PAD_HOR, 0);
  lv_obj_set_style_pad_ver(card, PAD_VER, 0);
  lv_obj_set_style_pad_row(card, TITLE_GAP, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  if (title && *title) theme::label(card, &font_m14, false, title);
  return card;
}

void close() {
  if (!dim) return;
  lv_obj_delete_async(dim);  // auch aus einem Ereignis des Fensters selbst sicher
  dim = nullptr;
  void (*cb)() = onClose;
  onClose = nullptr;
  if (cb) cb();
}

void setOnClose(void (*cb)()) { onClose = cb; }

bool isOpen() { return dim != nullptr; }

uint32_t generation() { return gen; }

lv_obj_t* addDoneButton(lv_obj_t* card, lv_event_cb_t onClick) {
  lv_obj_t* btn = lv_obj_create(card);
  lv_obj_remove_style_all(btn);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(btn, DONE_W, DONE_H);
  // Ausrichtung bezieht sich auf den Innenbereich der Karte, deshalb Innenabstand herausrechnen
  lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, PAD_HOR - DONE_RIGHT, DONE_TOP - PAD_VER);
  lv_obj_set_style_bg_color(btn, theme::c(theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(btn, theme::c(theme::LINE), LV_STATE_PRESSED);
  lv_obj_set_style_border_color(btn, theme::c(theme::LINE), 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_radius(btn, theme::RADIUS_TILE, 0);
  lv_obj_t* l = theme::label(btn, &font_m12, false, "Fertig");
  lv_obj_center(l);
  if (onClick) lv_obj_add_event_cb(btn, onClick, LV_EVENT_CLICKED, nullptr);
  return btn;
}

void tick(uint32_t nowMs, uint32_t lastTouchMs) {
  if (!dim || !autoCloseOn) return;
  const uint32_t since = (lastTouchMs > openedMs) ? lastTouchMs : openedMs;
  if (nowMs - since >= cfg::OVERLAY_TIMEOUT_MS) close();
}

}  // namespace overlay
