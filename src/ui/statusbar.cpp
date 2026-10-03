#include "statusbar.h"

#include <cstdio>
#include <cstring>

#include "config.h"
#include "ui/symbols.h"
#include "ui/theme.h"
#include "util/format.h"

namespace statusbar {

namespace {
// Maße aus der Vorschau (.sb)
constexpr int32_t PAD_X = 8;
constexpr int32_t GAP = 6;
constexpr int32_t DOT_SIZE = 6;
constexpr int32_t PDOT_SIZE = 5;
constexpr int32_t PDOT_GAP = 4;

lv_obj_t* bar;
lv_obj_t* linkDot;
lv_obj_t* nameLbl;
lv_obj_t* pdots[MAX_PAGES];
lv_obj_t* milLbl;
lv_obj_t* tankLbl;
lv_obj_t* clockLbl;
lv_timer_t* blinkTimer;

// zuletzt angezeigt, damit nur Änderungen gezeichnet werden
uint32_t shownDotColor = 0xFFFFFFFF;
bool blinking = false;
bool shownMil = false;
char shownTank[24] = "";
uint32_t shownTankColor = 0;
char shownClock[8] = "";

void blinkCb(lv_timer_t*) {
  const lv_opa_t o = lv_obj_get_style_bg_opa(linkDot, 0);
  lv_obj_set_style_bg_opa(linkDot, o == LV_OPA_COVER ? LV_OPA_20 : LV_OPA_COVER, 0);
}

lv_obj_t* circle(lv_obj_t* parent, int32_t size, uint32_t color) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_size(o, size, size);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(o, theme::c(color), 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}

void setText(lv_obj_t* l, char* shown, size_t shownSize, const char* text) {
  if (strcmp(shown, text) == 0) return;
  snprintf(shown, shownSize, "%s", text);
  lv_label_set_text(l, text);
}

void setVisible(lv_obj_t* o, bool on) {
  if (on == !lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
  if (on)
    lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}
}  // namespace

void create(lv_obj_t* parent, lv_event_cb_t onLongPress) {
  bar = lv_obj_create(parent);
  lv_obj_remove_style_all(bar);
  lv_obj_set_size(bar, BOARD_LCD_HOR_RES, theme::STATUSBAR_H);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_style_bg_color(bar, theme::c(theme::BG), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(bar, 1, 0);
  lv_obj_set_style_border_color(bar, theme::c(theme::LINE), 0);
  lv_obj_set_style_pad_hor(bar, PAD_X, 0);
  lv_obj_set_style_pad_column(bar, GAP, 0);
  lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(bar, LV_OBJ_FLAG_CLICKABLE);
  if (onLongPress) lv_obj_add_event_cb(bar, onLongPress, LV_EVENT_LONG_PRESSED, nullptr);

  linkDot = circle(bar, DOT_SIZE, theme::MUTED);
  nameLbl = theme::label(bar, &font_m12, false);

  lv_obj_t* dots = lv_obj_create(bar);
  lv_obj_remove_style_all(dots);
  lv_obj_set_height(dots, PDOT_SIZE);
  lv_obj_set_flex_grow(dots, 1);
  lv_obj_set_style_pad_column(dots, PDOT_GAP, 0);
  lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(dots, LV_OBJ_FLAG_CLICKABLE);
  for (auto& d : pdots) d = circle(dots, PDOT_SIZE, theme::LINE);

  milLbl = theme::label(bar, &font_m12, false, SYM_ENGINE);
  lv_obj_set_style_text_color(milLbl, theme::c(theme::WARN), 0);
  lv_obj_add_flag(milLbl, LV_OBJ_FLAG_HIDDEN);
  tankLbl = theme::label(bar, &font_m12, true);
  lv_obj_add_flag(tankLbl, LV_OBJ_FLAG_HIDDEN);
  clockLbl = theme::label(bar, &font_m12, true);
  lv_obj_add_flag(clockLbl, LV_OBJ_FLAG_HIDDEN);

  blinkTimer = lv_timer_create(blinkCb, cfg::STATUS_BLINK_MS, nullptr);
  lv_timer_pause(blinkTimer);
}

void setPage(const char* name, int index, int count) {
  lv_label_set_text(nameLbl, name);
  for (int i = 0; i < MAX_PAGES; i++) {
    setVisible(pdots[i], i < count);
    lv_obj_set_style_bg_color(pdots[i], theme::c(i == index ? theme::ACCENT : theme::LINE), 0);
  }
}

void update(const CarSnapshot& s) {
  // Verbindungspunkt: grün = Daten fließen, bernstein = verbunden, Motor aus (Vorschau),
  // grau blinkend = verbinde, grau = getrennt (U Rahmen)
  const bool connecting = s.link == LinkState::Searching || s.link == LinkState::Connecting ||
                          s.link == LinkState::InitAdapter;
  uint32_t dotColor = theme::MUTED;
  if (s.link == LinkState::Running) dotColor = s.engineRunning() ? theme::GOOD : theme::WARN;
  if (dotColor != shownDotColor) {
    shownDotColor = dotColor;
    lv_obj_set_style_bg_color(linkDot, theme::c(dotColor), 0);
  }
  if (connecting != blinking) {
    blinking = connecting;
    if (connecting) {
      lv_timer_resume(blinkTimer);
    } else {
      lv_timer_pause(blinkTimer);
      lv_obj_set_style_bg_opa(linkDot, LV_OPA_COVER, 0);
    }
  }

  // Motorkontrollleuchte
  const float mil = s.mil.get(s.now);
  const bool milOn = mil > 0.5f;  // NAN vergleicht immer falsch
  if (milOn != shownMil) {
    shownMil = milOn;
    setVisible(milLbl, milOn);
  }

  // Tanksymbol mit Litern; bernstein bei Reichweite unter 50 km (U Rahmen)
  const float tank = s.tankL.get(s.now);
  setVisible(tankLbl, !std::isnan(tank));
  if (!std::isnan(tank)) {
    char num[12], text[24];
    fmt::number(num, sizeof(num), tank, 0);
    snprintf(text, sizeof(text), SYM_PUMP " %s l", num);
    setText(tankLbl, shownTank, sizeof(shownTank), text);
    const float range = s.rangeKm.get(s.now);
    const uint32_t col = range < cfg::RANGE_LOW_KM ? theme::WARN : theme::MUTED;
    if (col != shownTankColor) {
      shownTankColor = col;
      lv_obj_set_style_text_color(tankLbl, theme::c(col), 0);
    }
  }

  // Uhrzeit nur mit GPS
  setVisible(clockLbl, s.hasGps && s.gpsTimeValid);
  if (s.hasGps && s.gpsTimeValid) {
    char text[8];
    snprintf(text, sizeof(text), "%02u:%02u", s.gpsHour, s.gpsMinute);
    setText(clockLbl, shownClock, sizeof(shownClock), text);
  }
}

}  // namespace statusbar
