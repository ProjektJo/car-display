// Seite "Diagramme" (U Seite 7): Linienverlauf mit Zeitwahl 1 / 5 / 30 min oben rechts, Wertewahl
// über die Chips unten (Verbrauch, Tempo, Drehzahl, Gas, Kühlmittel, Spannung). Höchstens zwei Linien,
// die zweite gestrichelt mit rechter Achse. Zeichnet höchstens einmal je Sekunde neu (neue Werte).
#include <cstdio>
#include <cstring>

#include "page.h"
#include "ui/history.h"
#include "ui/linechart.h"
#include "ui/theme.h"
#include "ui/ui_prefs.h"
#include "ui/values.h"

namespace {

using values::Series;

constexpr int WINDOWS[] = {60, 300, 1800};
const char* const WINDOW_LABELS[] = {"1 min", "5 min", "30 min"};
constexpr int WIN_COUNT = 3;
// Positionen aus der Vorschau (pDiagramme): Diagramm ab y 26, Fläche 30/6, 258 × 132
constexpr int32_t CHART_Y = 26, CHART_H = 150;
constexpr int32_t PLOT_X = 30, PLOT_Y = 6, PLOT_W = 258, PLOT_H = 132;
constexpr int32_t CHIP_H = 18;

Series seriesOf(int i) {
  values::Key k = values::Key::COUNT;
  if (!values::fromId(uiprefs::get().series[i], k)) return Series::None;
  return values::series(k);
}

// Reihe -> Wert-Schlüssel (für die Speicherung)
values::Key keyOf(Series r) {
  for (int k = 0; k < values::COUNT; k++)
    if (values::series(static_cast<values::Key>(k)) == r) return static_cast<values::Key>(k);
  return values::Key::COUNT;
}

lv_obj_t* chip(lv_obj_t* parent, const char* text, lv_event_cb_t cb, intptr_t user) {
  lv_obj_t* c = lv_obj_create(parent);
  lv_obj_remove_style_all(c);
  lv_obj_set_size(c, LV_SIZE_CONTENT, CHIP_H);
  lv_obj_set_style_radius(c, CHIP_H / 2, 0);
  lv_obj_set_style_border_width(c, 1, 0);
  lv_obj_set_style_border_color(c, theme::c(theme::LINE), 0);
  lv_obj_set_style_pad_hor(c, 6, 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(c, theme::c(theme::BG), 0);
  lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(c, 4);
  lv_obj_add_event_cb(c, cb, LV_EVENT_SHORT_CLICKED, reinterpret_cast<void*>(user));
  lv_obj_t* l = theme::label(c, &font_m12, true, text);
  lv_obj_center(l);
  return c;
}

void setChip(lv_obj_t* c, bool on) {
  lv_obj_set_style_bg_color(c, theme::c(on ? theme::ACCENT : theme::BG), 0);
  lv_obj_set_style_border_color(c, theme::c(on ? theme::ACCENT : theme::LINE), 0);
  lv_obj_set_style_text_color(lv_obj_get_child(c, 0), theme::c(on ? theme::BG : theme::MUTED), 0);
}

class ChartsPage : public Page {
 public:
  ChartsPage() : Page("Diagramme") {}

  void create(lv_obj_t* parent) override {
    title_ = theme::label(parent, &font_m12, false, "");
    lv_obj_set_pos(title_, 10, 6);
    lv_obj_set_style_text_color(title_, theme::c(theme::ACCENT), 0);
    sub_ = theme::label(parent, &font_m12, true, "");
    lv_obj_align_to(sub_, title_, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

    lv_obj_t* wins = row(parent, 3);
    lv_obj_align(wins, LV_ALIGN_TOP_RIGHT, -8, 4);
    for (int i = 0; i < WIN_COUNT; i++) winChip_[i] = chip(wins, WINDOW_LABELS[i], onWindow, i);

    chart_ = lv_obj_create(parent);
    lv_obj_remove_style_all(chart_);
    lv_obj_set_pos(chart_, 0, CHART_Y);
    lv_obj_set_size(chart_, BOARD_LCD_HOR_RES, CHART_H);
    lv_obj_remove_flag(chart_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(chart_, onDraw, LV_EVENT_DRAW_MAIN, this);

    since_ = theme::label(parent, &font_m12, true, "");
    lv_obj_set_pos(since_, 34, CHART_Y + PLOT_Y + PLOT_H - 16);

    lv_obj_t* sers = row(parent, 0);
    lv_obj_set_width(sers, BOARD_LCD_HOR_RES - 12);
    lv_obj_set_flex_align(sers, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(sers, LV_ALIGN_BOTTOM_MID, 0, -8);
    for (int i = 0; i < values::SERIES_COUNT; i++)
      serChip_[i] = chip(sers, values::seriesShort(static_cast<Series>(i)), onSeries, i);
    refreshChips();
  }

  void onShow() override {
    refreshChips();
    lv_obj_invalidate(chart_);
  }

  void update(const CarSnapshot&) override {
    if (history::seq() == drawnSeq_) return;
    drawnSeq_ = history::seq();
    lv_obj_invalidate(chart_);
    // Hinweis, solange noch nicht das ganze Zeitfenster aufgezeichnet ist
    char t[32] = "";
    const int win = window();
    if (history::count() < win) snprintf(t, sizeof(t), "Daten seit %d min", (history::count() + 30) / 60);
    if (strcmp(t, shownSince_) != 0) {
      snprintf(shownSince_, sizeof(shownSince_), "%s", t);
      lv_label_set_text(since_, t);
    }
  }

 private:
  static int window() {
    const int w = uiprefs::get().windowS;
    for (int x : WINDOWS)
      if (x == w) return w;
    return 300;
  }

  static lv_obj_t* row(lv_obj_t* parent, int32_t gap) {
    lv_obj_t* r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(r, gap, 0);
    lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    return r;
  }

  void refreshChips() {
    const int win = window();
    for (int i = 0; i < WIN_COUNT; i++) setChip(winChip_[i], WINDOWS[i] == win);
    const Series a = seriesOf(0), b = seriesOf(1);
    for (int i = 0; i < values::SERIES_COUNT; i++) setChip(serChip_[i], static_cast<Series>(i) == a || static_cast<Series>(i) == b);
    lv_label_set_text(title_, a == Series::None ? "" : values::seriesLabel(a));
    char t[48] = "";
    if (b != Series::None) snprintf(t, sizeof(t), " \xC2\xB7 %s", values::seriesLabel(b));  // zweite Linie gestrichelt
    lv_label_set_text(sub_, t);
    lv_obj_align_to(sub_, title_, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    lv_obj_invalidate(chart_);
  }

  static void onWindow(lv_event_t* e) {
    auto* p = static_cast<ChartsPage*>(chartsSelf());
    uiprefs::get().windowS = static_cast<uint16_t>(WINDOWS[reinterpret_cast<intptr_t>(lv_event_get_user_data(e))]);
    uiprefs::save();
    p->refreshChips();
    p->drawnSeq_ = 0;
  }

  // Tippen auf eine Reihe: an/aus; höchstens zwei, eine neue ersetzt die zweite
  static void onSeries(lv_event_t* e) {
    auto* p = static_cast<ChartsPage*>(chartsSelf());
    const Series r = static_cast<Series>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    Series a = seriesOf(0), b = seriesOf(1);
    if (r == a && b != Series::None) {
      a = b;
      b = Series::None;
    } else if (r == b) {
      b = Series::None;
    } else if (r != a) {
      if (a == Series::None) a = r;
      else b = r;
    }
    UiSettings& u = uiprefs::get();
    const values::Key ka = keyOf(a), kb = keyOf(b);
    snprintf(u.series[0], sizeof(u.series[0]), "%s", ka == values::Key::COUNT ? "" : values::id(ka));
    snprintf(u.series[1], sizeof(u.series[1]), "%s", kb == values::Key::COUNT ? "" : values::id(kb));
    uiprefs::save();
    p->refreshChips();
  }

  static void onDraw(lv_event_t* e) {
    linechart::Spec sp;
    sp.series[0] = seriesOf(0);
    sp.series[1] = seriesOf(1);
    sp.windowS = window();
    sp.x = PLOT_X;
    sp.y = PLOT_Y;
    sp.w = PLOT_W;
    sp.h = PLOT_H;
    linechart::draw(lv_event_get_layer(e), lv_event_get_target_obj(e), sp);
  }

  static Page* chartsSelf();

  lv_obj_t* title_ = nullptr;
  lv_obj_t* sub_ = nullptr;
  lv_obj_t* chart_ = nullptr;
  lv_obj_t* since_ = nullptr;
  lv_obj_t* winChip_[WIN_COUNT] = {};
  lv_obj_t* serChip_[values::SERIES_COUNT] = {};
  uint32_t drawnSeq_ = 0;
  char shownSince_[32] = "";
};

}  // namespace

Page* chartsPage() {
  static ChartsPage page;
  return &page;
}

Page* ChartsPage::chartsSelf() { return chartsPage(); }
