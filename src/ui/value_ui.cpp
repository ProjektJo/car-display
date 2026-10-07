#include "value_ui.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui/history.h"
#include "ui/linechart.h"
#include "ui/overlay.h"
#include "ui/theme.h"
#include "util/format.h"

namespace valueui {

namespace {

using values::Key;

// Detail: Diagramm der letzten 5 min (Vorschau: 270 × 100 ab 12/70, Zeichenfläche 22/6, 244 × 84)
constexpr int DETAIL_WINDOW_S = 300;
constexpr int32_t DETAIL_CHART_Y = 62, DETAIL_CHART_H = 100;
constexpr int32_t PICK_ROW_H = 28;  // Auswahl: Zeilen fingerfreundlich hoch

struct Detail {
  Key key = Key::Speed;
  uint32_t gen = 0;
  lv_obj_t* value = nullptr;
  lv_obj_t* unit = nullptr;
  lv_obj_t* chart = nullptr;
  lv_obj_t* minmax = nullptr;
  uint32_t drawnSeq = 0;
  char shown[24] = "";
} detail;

PickCb pickCb = nullptr;
void* pickUser = nullptr;

void onDetailClose(lv_event_t*) { overlay::close(); }

void detailDraw(lv_event_t* e) {
  linechart::Spec sp;
  sp.series[0] = values::series(detail.key);
  sp.windowS = DETAIL_WINDOW_S;
  sp.x = 30;
  sp.y = 8;
  sp.w = 236;
  sp.h = DETAIL_CHART_H - 16;
  linechart::draw(lv_event_get_layer(e), lv_event_get_target_obj(e), sp);
}

void onPick(lv_event_t* e) {
  const Key k = static_cast<Key>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  PickCb cb = pickCb;
  void* user = pickUser;
  overlay::close();
  if (cb) cb(k, user);
}

}  // namespace

void openDetail(Key k) {
  lv_obj_t* card = overlay::open("");
  detail.key = k;
  detail.gen = overlay::generation();
  detail.shown[0] = '\0';
  detail.drawnSeq = 0;
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(card, onDetailClose, LV_EVENT_CLICKED, nullptr);  // Tippen schließt
  lv_obj_add_event_cb(lv_obj_get_parent(card), onDetailClose, LV_EVENT_CLICKED, nullptr);
  lv_obj_set_layout(card, LV_LAYOUT_NONE);
  lv_obj_t* t = theme::label(card, &font_m14, false, values::label(k));
  lv_obj_set_pos(t, 0, 0);
  const bool hasSeries = values::series(k) != values::Series::None;
  if (hasSeries) {
    lv_obj_t* sub = theme::label(card, &font_m12, true, "letzte 5 min");
    lv_obj_align_to(sub, t, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, 0);
  }
  lv_obj_t* row = lv_obj_create(card);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_pos(row, 0, 18);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_column(row, 5, 0);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
  detail.value = theme::label(row, &font_v40, false, "");
  detail.unit = theme::label(row, &font_m14, true, "");
  lv_obj_set_style_pad_bottom(detail.unit, 7, 0);
  if (hasSeries) {
    detail.chart = lv_obj_create(card);
    lv_obj_remove_style_all(detail.chart);
    lv_obj_set_pos(detail.chart, 0, DETAIL_CHART_Y);
    lv_obj_set_size(detail.chart, LV_PCT(100), DETAIL_CHART_H);
    lv_obj_remove_flag(detail.chart, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(detail.chart, detailDraw, LV_EVENT_DRAW_MAIN, nullptr);
    detail.minmax = theme::label(card, &font_m12, true, "");
    lv_obj_align(detail.minmax, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  } else {
    detail.chart = nullptr;
    detail.minmax = nullptr;
    lv_obj_t* note = theme::label(card, &font_m12, true, "Tippen schließt.");
    lv_obj_align(note, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  }
}

void openPicker(const char* title, Key current, PickCb cb, void* user) {
  pickCb = cb;
  pickUser = user;
  lv_obj_t* card = overlay::open(title);
  lv_obj_t* grid = lv_obj_create(card);
  lv_obj_remove_style_all(grid);
  lv_obj_set_width(grid, LV_PCT(100));
  lv_obj_set_flex_grow(grid, 1);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_column(grid, 8, 0);
  lv_obj_set_scroll_dir(grid, LV_DIR_VER);  // mehr Werte als Platz: wischen
  lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_ACTIVE);
  lv_obj_update_layout(card);
  const int32_t colW = (lv_obj_get_content_width(card) - 8) / 2;
  lv_obj_t* currentRow = nullptr;
  for (int i = 0; i < values::COUNT; i++) {
    lv_obj_t* r = lv_obj_create(grid);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, colW, PICK_ROW_H);
    lv_obj_set_style_pad_hor(r, 2, 0);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_set_style_border_color(r, theme::c(theme::LINE), 0);
    lv_obj_set_style_bg_color(r, theme::c(theme::LINE), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(r, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_event_cb(r, onPick, LV_EVENT_SHORT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
    lv_obj_t* l = theme::label(r, &font_m14, false, values::label(static_cast<Key>(i)));
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
    if (static_cast<Key>(i) == current) {
      lv_obj_set_style_text_color(l, theme::c(theme::ACCENT), 0);
      currentRow = r;
    }
  }
  if (currentRow) lv_obj_scroll_to_view(currentRow, LV_ANIM_OFF);
}

void tick(const CarSnapshot& s) {
  if (!overlay::isOpen() || overlay::generation() != detail.gen || !detail.value) return;
  char t[24];
  values::text(detail.key, s, t, sizeof(t));
  if (strcmp(t, detail.shown) != 0) {
    snprintf(detail.shown, sizeof(detail.shown), "%s", t);
    lv_label_set_text(detail.value, t);
    lv_obj_set_style_text_color(detail.value, theme::c(values::color(detail.key, s)), 0);
    lv_label_set_text(detail.unit, values::unit(detail.key, s));
  }
  if (detail.chart && history::seq() != detail.drawnSeq) {
    detail.drawnSeq = history::seq();
    lv_obj_invalidate(detail.chart);
    float lo = 0, hi = 0;
    if (linechart::minMax(values::series(detail.key), DETAIL_WINDOW_S, lo, hi)) {
      char a[16], b[16], m[64];
      const int d = values::decimals(detail.key);
      fmt::number(a, sizeof(a), lo, d);
      fmt::number(b, sizeof(b), hi, d);
      snprintf(m, sizeof(m), "Min %s " "\xC2\xB7" " Max %s " "\xC2\xB7" " Tippen schließt", a, b);
      lv_label_set_text(detail.minmax, m);
    }
  }
}

}  // namespace valueui
