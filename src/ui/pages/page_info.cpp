// Seite "Info" (U Seite 11): Kurzanleitung mit vier Chips Bedienung, Farben, Werte, Eco-Score;
// unten Version, Fahrzeugprofil, Adapter und Abfragen pro Sekunde. Texte aus der Vorschau (pInfo).
#include <cmath>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "page.h"
#include "ui/theme.h"
#include "util/format.h"

namespace {

constexpr int TABS = 4;
constexpr int ROWS = 6;
const char* const TAB_NAMES[TABS] = {"Bedienung", "Farben", "Werte", "Eco-Score"};

struct Row {
  const char* key;
  const char* text;
  uint32_t keyColor;
};

const Row CONTENT[TABS][ROWS] = {
    {{"Wischen", "Seite wechseln", theme::TEXT},
     {"Tippen auf Kachel", "Verlauf 5 min", theme::TEXT},
     {"Lang auf Kachel", "Wert wählen", theme::TEXT},
     {"Lang auf Fläche", "Menü", theme::TEXT},
     {"BOOT-Taste", "nächste Seite, lang: Menü", theme::TEXT},
     {"Tank-Fenster", "\xE2\x96\xB2\xE2\x96\xBC je Stelle, Ziffer tippen", theme::TEXT}},
    {{"Grün", "sparsamer als Ziel bzw. Schnitt, Score 80+", theme::GOOD},
     {"Weiß", "normal", theme::TEXT},
     {"Bernstein", "über Ziel bzw. Schnitt, kalter Motor", theme::WARN},
     {"Blau", "Bezugslinie, Bedienung", theme::ACCENT},
     {"Rot", "nur Fehlercodes", theme::BAD},
     {nullptr, nullptr, 0}},
    {{"Tank", "Schnitt seit letztem Tanken", theme::TEXT},
     {"100 / 10 / 1 km", "Schnitt der letzten Strecke", theme::TEXT},
     {"Momentan", "1-Sekunden-Wert", theme::TEXT},
     {"SCHUB", "Motor spritzt nicht ein", theme::TEXT},
     {"Reichweite", "Tankinhalt \xC3\xB7 Prognose", theme::TEXT},
     {"Gebremst", "durch Bremsen verlorene Energie", theme::TEXT}},
    {{"Schub und Rollen", "35 % \xC2\xB7 Gas weg statt bremsen", theme::TEXT},
     {"Ruhiges Gas", "25 % \xC2\xB7 wenig Pedal-Zappeln", theme::TEXT},
     {"Früh schalten", "20 % \xC2\xB7 beim Pfeil hochschalten", theme::TEXT},
     {"Sanft bremsen", "10 % \xC2\xB7 wenig Gebremst", theme::TEXT},
     {"Wenig Leerlauf", "10 % \xC2\xB7 Motor aus im Stand", theme::TEXT},
     {"80+ gut \xC2\xB7 60\xE2\x80\x93" "79 ok", "unter 60 Luft nach oben", theme::GOOD}},
};

class InfoPage : public Page {
 public:
  InfoPage() : Page("Info") {}

  void create(lv_obj_t* parent) override {
    lv_obj_t* tabs = lv_obj_create(parent);
    lv_obj_remove_style_all(tabs);
    lv_obj_set_size(tabs, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(tabs, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_flex_flow(tabs, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(tabs, 4, 0);
    lv_obj_remove_flag(tabs, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < TABS; i++) {
      lv_obj_t* c = lv_obj_create(tabs);
      lv_obj_remove_style_all(c);
      lv_obj_set_size(c, LV_SIZE_CONTENT, 20);
      lv_obj_set_style_radius(c, 10, 0);
      lv_obj_set_style_border_width(c, 1, 0);
      lv_obj_set_style_pad_hor(c, 9, 0);
      lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
      lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_ext_click_area(c, 4);
      lv_obj_add_event_cb(c, onTab, LV_EVENT_SHORT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
      lv_obj_t* l = theme::label(c, &font_m12, false, TAB_NAMES[i]);
      lv_obj_center(l);
      chips_[i] = c;
    }
    for (int r = 0; r < ROWS; r++) {
      lv_obj_t* row = lv_obj_create(parent);
      lv_obj_remove_style_all(row);
      lv_obj_set_pos(row, 12, 30 + r * 24);
      lv_obj_set_size(row, BOARD_LCD_HOR_RES - 24, 24);
      lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
      lv_obj_set_style_border_width(row, 1, 0);
      lv_obj_set_style_border_color(row, theme::c(theme::LINE), 0);
      lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
      key_[r] = theme::label(row, &font_m12, false, "");
      lv_obj_align(key_[r], LV_ALIGN_LEFT_MID, 2, 0);
      text_[r] = theme::label(row, &font_small, true, "");
      lv_obj_align(text_[r], LV_ALIGN_RIGHT_MID, -2, 0);
      rows_[r] = row;
    }
    foot1_ = theme::label(parent, &font_small, true, "");
    lv_obj_align(foot1_, LV_ALIGN_BOTTOM_LEFT, 12, -6);
    foot2_ = theme::label(parent, &font_small, true, "");
    lv_obj_align(foot2_, LV_ALIGN_BOTTOM_RIGHT, -12, -6);
    char v[32];
    snprintf(v, sizeof(v), "Car-Display %s", cfg::FW_VERSION);
    lv_label_set_text(foot1_, v);
    showTab(0);
  }

  void update(const CarSnapshot& s) override {
    char rate[16], t[96];
    fmt::number(rate, sizeof(rate), s.link.queriesPerS, 1);
    snprintf(t, sizeof(t), "%s \xC2\xB7 %s \xC2\xB7 %s Abfr./s", s.profile.name[0] ? s.profile.name : "kein Profil",
             s.simulated ? "Simulator" : (s.link.adapter[0] ? s.link.adapter : "kein Adapter"), rate);
    if (strcmp(t, shownFoot_) != 0) {
      snprintf(shownFoot_, sizeof(shownFoot_), "%s", t);
      lv_label_set_text(foot2_, t);
    }
  }

 private:
  void showTab(int tab) {
    tab_ = tab;
    for (int i = 0; i < TABS; i++) {
      const bool on = i == tab;
      lv_obj_set_style_bg_color(chips_[i], theme::c(on ? theme::ACCENT : theme::BG), 0);
      lv_obj_set_style_border_color(chips_[i], theme::c(on ? theme::ACCENT : theme::LINE), 0);
      lv_obj_set_style_text_color(lv_obj_get_child(chips_[i], 0), theme::c(on ? theme::BG : theme::MUTED), 0);
    }
    for (int r = 0; r < ROWS; r++) {
      const Row& row = CONTENT[tab][r];
      if (!row.key) {
        lv_obj_add_flag(rows_[r], LV_OBJ_FLAG_HIDDEN);
        continue;
      }
      lv_obj_remove_flag(rows_[r], LV_OBJ_FLAG_HIDDEN);
      lv_label_set_text_static(key_[r], row.key);
      lv_obj_set_style_text_color(key_[r], theme::c(row.keyColor), 0);
      lv_label_set_text_static(text_[r], row.text);
      // Eco-Score: "unter 60 …" in warn
      lv_obj_set_style_text_color(text_[r], theme::c(tab == 3 && r == ROWS - 1 ? theme::WARN : theme::MUTED), 0);
    }
  }

  static void onTab(lv_event_t* e) {
    static_cast<InfoPage*>(infoSelf())->showTab(static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e))));
  }
  static Page* infoSelf();

  lv_obj_t* chips_[TABS] = {};
  lv_obj_t* rows_[ROWS] = {};
  lv_obj_t* key_[ROWS] = {};
  lv_obj_t* text_[ROWS] = {};
  lv_obj_t* foot1_ = nullptr;
  lv_obj_t* foot2_ = nullptr;
  int tab_ = 0;
  char shownFoot_[96] = "";
};

}  // namespace

Page* infoPage() {
  static InfoPage page;
  return &page;
}

Page* InfoPage::infoSelf() { return infoPage(); }
