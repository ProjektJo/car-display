#include "menu.h"

#include <cstdio>
#include <cstring>

#include "config.h"
#include "ui/overlay.h"
#include "ui/theme.h"
#include "util/format.h"
#include "util/link_text.h"

namespace menu {

namespace {

// Maße aus der Vorschau (.mrow, Menü zweispaltig mit 12 px Abstand)
constexpr int32_t ROW_PAD_VER = 4;
constexpr int32_t ROW_PAD_HOR = 2;
constexpr int32_t COL_GAP = 12;
constexpr int32_t DIAG_VALUE_W = 172;  // rechte Spalte im Diagnose-Dialog, längere Texte brechen um

enum class Shown : uint8_t { None, Menu, Diagnose };
Shown shown = Shown::None;
uint32_t shownGen = 0;

lv_obj_t* menuDiagValue = nullptr;  // Menüzeile Diagnose: "8,0 Abfr./s"
char menuDiagShown[24] = "";
enum DiagRow { ROW_ADAPTER, ROW_PROTOCOL, ROW_RATE, ROW_PIDS, ROW_FUEL, ROW_BODY, ROW_COUNT };
lv_obj_t* diagValues[ROW_COUNT] = {};
char diagShown[ROW_COUNT][48] = {};

bool stillOpen(Shown what) { return shown == what && overlay::isOpen() && overlay::generation() == shownGen; }

void setText(lv_obj_t* l, char* shownText, size_t size, const char* text) {
  if (strcmp(shownText, text) == 0) return;
  snprintf(shownText, size, "%s", text);
  lv_label_set_text(l, text);
}

// Zeile mit Text links und Wert rechts (muted), Linie darunter
lv_obj_t* row(lv_obj_t* parent, const char* key, lv_obj_t** valueOut, int32_t width) {
  lv_obj_t* r = lv_obj_create(parent);
  lv_obj_remove_style_all(r);
  lv_obj_set_width(r, width);
  lv_obj_set_height(r, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_ver(r, ROW_PAD_VER, 0);
  lv_obj_set_style_pad_hor(r, ROW_PAD_HOR, 0);
  lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(r, 1, 0);
  lv_obj_set_style_border_color(r, theme::c(theme::LINE), 0);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  theme::label(r, &font_m12, false, key);
  *valueOut = theme::label(r, &font_m12, true, "");
  return r;
}

void onDone(lv_event_t*) { overlay::close(); }
void onBackToMenu(lv_event_t*) { open(); }
void onDiagRow(lv_event_t*) { openDiagnose(); }

void rateText(const CarSnapshot& s, char* out, size_t size, const char* unit) {
  if (std::isnan(s.link.queriesPerS)) {
    snprintf(out, size, "%s", fmt::NO_VALUE);
    return;
  }
  char num[12];
  fmt::number(num, sizeof(num), s.link.queriesPerS, 1);
  snprintf(out, size, "%s %s", num, unit);
}

}  // namespace

void open() {
  lv_obj_t* card = overlay::open("Menü");
  shown = Shown::Menu;
  shownGen = overlay::generation();
  overlay::addDoneButton(card, onDone);
  // Daneben tippen schließt ebenfalls
  lv_obj_add_event_cb(lv_obj_get_parent(card), onDone, LV_EVENT_CLICKED, nullptr);

  // Zweispaltig wie in der Vorschau; Etappe 2 hat nur die Zeile Diagnose
  lv_obj_t* grid = lv_obj_create(card);
  lv_obj_remove_style_all(grid);
  lv_obj_set_width(grid, LV_PCT(100));
  lv_obj_set_height(grid, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_column(grid, COL_GAP, 0);
  lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_update_layout(card);
  const int32_t colW = (lv_obj_get_content_width(card) - COL_GAP) / 2;
  menuDiagShown[0] = '\0';
  lv_obj_t* r = row(grid, "Diagnose", &menuDiagValue, colW);
  lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(r, theme::c(theme::LINE), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_add_event_cb(r, onDiagRow, LV_EVENT_CLICKED, nullptr);

  theme::label(card, &font_m12, true, "Die übrigen Einträge folgen in Etappe 7.");
}

void openDiagnose() {
  lv_obj_t* card = overlay::open("Diagnose", true, theme::DIALOG_INSET_SUB);
  shown = Shown::Diagnose;
  shownGen = overlay::generation();
  overlay::addDoneButton(card, onBackToMenu);
  memset(diagShown, 0, sizeof(diagShown));

  static const char* const KEYS[ROW_COUNT] = {"Adapter", "Protokoll", "Abfragen", "Unterstützte PIDs", "Verbrauch aus",
                                              "Fahrzeugart"};
  lv_obj_set_style_pad_row(card, 0, 0);
  lv_obj_t* title = lv_obj_get_child(card, 0);
  lv_obj_set_style_margin_bottom(title, 10, 0);
  for (int i = 0; i < ROW_COUNT; i++) {
    row(card, KEYS[i], &diagValues[i], LV_PCT(100));
    lv_obj_set_width(diagValues[i], DIAG_VALUE_W);
    lv_obj_set_style_text_align(diagValues[i], LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(diagValues[i], LV_LABEL_LONG_WRAP);
  }
}

void update(const CarSnapshot& s) {
  char text[48];
  if (stillOpen(Shown::Menu)) {
    rateText(s, text, sizeof(text), "Abfr./s");
    setText(menuDiagValue, menuDiagShown, sizeof(menuDiagShown), text);
    return;
  }
  if (!stillOpen(Shown::Diagnose)) {
    shown = Shown::None;
    return;
  }
  const LinkInfo& li = s.link;
  // Adapter, Protokoll
  if (li.adapter[0])
    snprintf(text, sizeof(text), "%s (BLE)", li.adapter);
  else
    snprintf(text, sizeof(text), "%s", s.link.state == LinkState::Searching ? "Suche …" : fmt::NO_VALUE);
  setText(diagValues[ROW_ADAPTER], diagShown[ROW_ADAPTER], sizeof(diagShown[0]), text);
  setText(diagValues[ROW_PROTOCOL], diagShown[ROW_PROTOCOL], sizeof(diagShown[0]), li.protocol[0] ? li.protocol : fmt::NO_VALUE);
  // Abfragen pro Sekunde (A7)
  rateText(s, text, sizeof(text), "pro Sekunde");
  setText(diagValues[ROW_RATE], diagShown[ROW_RATE], sizeof(diagShown[0]), text);
  // Unterstützte PIDs
  linktext::supported(li, text, sizeof(text));
  setText(diagValues[ROW_PIDS], diagShown[ROW_PIDS], sizeof(diagShown[0]), text);
  // Verbrauchsquelle mit Kalibrierfaktor (fuel_cal ab Etappe 3 aus dem Profil)
  char cal[12];
  fmt::number(cal, sizeof(cal), cfg::DEFAULT_FUEL_CAL, 2);
  if (li.supportedKnown)
    snprintf(text, sizeof(text), "%s, Kalibrierung %s", linktext::fuelSource(li), cal);
  else
    snprintf(text, sizeof(text), "%s", fmt::NO_VALUE);
  setText(diagValues[ROW_FUEL], diagShown[ROW_FUEL], sizeof(diagShown[0]), text);
  // Fahrzeugart (im Menü wählbar ab Etappe 7)
  char mass[12];
  fmt::number(mass, sizeof(mass), cfg::DEFAULT_BODY_MASS_KG, 0);
  snprintf(text, sizeof(text), "%s, %s kg", cfg::DEFAULT_BODY_NAME, mass);
  setText(diagValues[ROW_BODY], diagShown[ROW_BODY], sizeof(diagShown[0]), text);
}

}  // namespace menu
