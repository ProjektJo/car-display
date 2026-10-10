#include "menu.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "core/commands.h"
#include "sensors/sensor_task.h"
#include "storage/storage_task.h"
#include "hw/display.h"
#include "hw/touch.h"
#include "ui/numpad.h"
#include "ui/overlay.h"
#include "ui/symbols.h"
#include "ui/tank_dialog.h"
#include "ui/theme.h"
#include "ui/ui.h"
#include "ui/ui_prefs.h"
#include "ui/vehicle_dialog.h"
#include "util/format.h"
#include "util/link_text.h"

namespace menu {

namespace {

// Maße aus der Vorschau (.mrow, Menü zweispaltig mit 12 px Abstand)
constexpr int32_t ROW_PAD_VER = 9;   // 7.10.2026 größer (vorher 4)
constexpr int32_t ROW_PAD_HOR = 2;
constexpr int32_t COL_GAP = 8;
constexpr int32_t DIAG_VALUE_W = 172;  // rechte Spalte im Diagnose-Dialog, längere Texte brechen um
constexpr int32_t STEP_BTN = 40;       // große − / + Tasten (U Menü)
constexpr uint16_t COLD_RPM_MIN = 2000, COLD_RPM_MAX = 3000, COLD_RPM_STEP = 250;  // Kalt-Grenze (U Menü)

enum class Shown : uint8_t { None, Menu, Diagnose, Brightness, Goal, Body, Maintenance };
Shown shown = Shown::None;
uint32_t shownGen = 0;
CarSnapshot snap;  // letzter Stand für Dialoge

// ---------- Menü-Zeilen ----------
enum MenuRow {
  M_BRIGHT, M_GOAL, M_BODY, M_MAINT, M_DIAG, M_REFUEL,      // linke Spalte
  M_TILES, M_TIPS, M_COLD, M_SPRINT, M_ENDTRIP, M_INFO,      // rechte Spalte
  M_COUNT
};
const char* const MENU_KEYS[M_COUNT] = {"Helligkeit", "Spar-Ziel", "Fahrzeug", "Wartung", "Diagnose", "Getankt",
                                        "Kacheln zurücksetzen", "Spartipps", "Kalt-Grenze", "Auto-Sprint", "Fahrt beenden",
                                        "Info"};
// Reihenfolge im Menü: oben häufig, unten selten (Jos Wunsch)
constexpr int MENU_ORDER[M_COUNT] = {M_BRIGHT, M_REFUEL, M_TIPS, M_SPRINT, M_ENDTRIP, M_GOAL,
                                     M_MAINT, M_DIAG, M_COLD, M_BODY, M_TILES, M_INFO};
constexpr int FREQUENT_COUNT = 6;
constexpr int32_t MENU_ROW_H = 44;
// Symbole: LVGL-Symbole aus Montserrat 24, Zapfsäule/Tropfen/Thermometer aus font_m20 (Ersatzschrift)
const char* const MENU_ICONS[M_COUNT] = {LV_SYMBOL_EYE_OPEN, SYM_DROP, LV_SYMBOL_EDIT, LV_SYMBOL_SETTINGS, LV_SYMBOL_LIST,
                                         SYM_PUMP, LV_SYMBOL_REFRESH, LV_SYMBOL_BELL, SYM_THERMO, LV_SYMBOL_CHARGE,
                                         LV_SYMBOL_STOP, LV_SYMBOL_FILE};
lv_obj_t* menuPills[M_COUNT] = {};
lv_obj_t* menuValues[M_COUNT] = {};
char menuShown[M_COUNT][32] = {};

enum DiagRow { ROW_VEHICLE, ROW_ADAPTER, ROW_PROTOCOL, ROW_VIN, ROW_RATE, ROW_PIDS, ROW_FUEL, ROW_CUT, ROW_BODY, ROW_RESET, ROW_SENSORS,
               ROW_IMU, ROW_EXPORT, ROW_COUNT };
lv_obj_t* diagValues[ROW_COUNT] = {};
char diagShown[ROW_COUNT][64] = {};

// Dialog-Inhalte, die sich live ändern
lv_obj_t* dlgValue[4] = {};
char dlgShown[4][48] = {};

bool stillOpen(Shown what) { return shown == what && overlay::isOpen() && overlay::generation() == shownGen; }

void setText(lv_obj_t* l, char* shownText, size_t size, const char* text) {
  if (!l || strcmp(shownText, text) == 0) return;
  snprintf(shownText, size, "%s", text);
  lv_label_set_text(l, text);
}

// Schalter im Menü: grünes Feld AN, graues AUS
void setToggle(int r, bool on) {
  if (strcmp(menuShown[r], on ? "AN" : "AUS") == 0) return;
  setText(menuValues[r], menuShown[r], sizeof(menuShown[0]), on ? "AN" : "AUS");
  if (menuPills[r]) lv_obj_set_style_bg_color(menuPills[r], theme::c(on ? theme::GOOD : theme::LINE), 0);
  lv_obj_set_style_text_color(menuValues[r], theme::c(on ? theme::BG : theme::MUTED), 0);
}

void pressable(lv_obj_t* o) {
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(o, theme::c(theme::LINE), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_STATE_PRESSED);
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
  theme::label(r, &font_m14, false, key);
  *valueOut = theme::label(r, &font_m14, true, "");
  return r;
}

lv_obj_t* button(lv_obj_t* parent, const char* text, int32_t w, int32_t h, lv_event_cb_t cb, void* user = nullptr,
                 bool accent = false) {
  lv_obj_t* b = lv_obj_create(parent);
  lv_obj_remove_style_all(b);
  lv_obj_set_size(b, w, h);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(b, theme::c(theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, theme::c(theme::LINE), LV_STATE_PRESSED);
  lv_obj_set_style_border_color(b, theme::c(accent ? theme::ACCENT : theme::LINE), 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_radius(b, theme::RADIUS_TILE, 0);
  lv_obj_t* l = theme::label(b, &font_m14, false, text);
  if (accent) lv_obj_set_style_text_color(l, theme::c(theme::ACCENT), 0);
  lv_obj_center(l);
  if (cb) {
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
    lv_obj_add_event_cb(b, cb, LV_EVENT_LONG_PRESSED_REPEAT, user);
  }
  return b;
}

lv_obj_t* chip(lv_obj_t* parent, const char* text, bool on, bool enabled, lv_event_cb_t cb, intptr_t user) {
  lv_obj_t* c = lv_obj_create(parent);
  lv_obj_remove_style_all(c);
  lv_obj_set_size(c, LV_SIZE_CONTENT, 32);
  lv_obj_set_style_radius(c, 16, 0);
  lv_obj_set_style_border_width(c, 1, 0);
  lv_obj_set_style_border_color(c, theme::c(on ? theme::ACCENT : theme::LINE), 0);
  lv_obj_set_style_bg_color(c, theme::c(on ? theme::ACCENT : theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(c, 14, 0);
  lv_obj_t* l = theme::label(c, &font_m14, false, text);
  lv_obj_set_style_text_color(l, theme::c(on ? theme::BG : (enabled ? theme::TEXT : theme::MUTED)), 0);
  lv_obj_center(l);
  if (enabled) {
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(c, cb, LV_EVENT_CLICKED, reinterpret_cast<void*>(user));
  }
  return c;
}

lv_obj_t* flexRow(lv_obj_t* parent, int32_t gap) {
  lv_obj_t* r = lv_obj_create(parent);
  lv_obj_remove_style_all(r);
  lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(r, gap, 0);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
  return r;
}

lv_obj_t* subDialog(const char* title, Shown what) {
  lv_obj_t* card = overlay::open(title, true, theme::DIALOG_INSET_SUB);
  shown = what;
  shownGen = overlay::generation();
  overlay::addDoneButton(card, [](lv_event_t*) { open(); });
  lv_obj_set_style_pad_row(card, 8, 0);
  lv_obj_add_flag(card, LV_OBJ_FLAG_SCROLLABLE);  // längere Dialoge: wischen
  lv_obj_set_scroll_dir(card, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_ACTIVE);
  memset(dlgShown, 0, sizeof(dlgShown));
  for (auto& v : dlgValue) v = nullptr;
  return card;
}

void rateText(const CarSnapshot& s, char* out, size_t size, const char* unit) {
  if (std::isnan(s.link.queriesPerS)) {
    snprintf(out, size, "%s", fmt::NO_VALUE);
    return;
  }
  char num[12];
  fmt::number(num, sizeof(num), s.link.queriesPerS, 1);
  snprintf(out, size, "%s %s", num, unit);
}

void kmText(char* out, size_t size, float km) {
  char n[16];
  fmt::number(n, sizeof(n), km, 0);
  snprintf(out, size, std::isnan(km) ? "%s" : "%s km", n);
}

// ---------- Helligkeit (U Menü) ----------
void openBrightness();

void onBrightMode(lv_event_t* e) {
  uiprefs::get().dayNight = static_cast<uint8_t>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  uiprefs::save();
  ui::applyBrightness();
  openBrightness();
}

void onBrightStep(lv_event_t* e) {
  const int code = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));  // ±1 Tag, ±2 Nacht
  UiSettings& u = uiprefs::get();
  if (code == 1 || code == -1) {
    const int v = u.brightDay + code * cfg::BRIGHT_DAY_STEP;
    u.brightDay = static_cast<uint8_t>(v < cfg::BRIGHT_DAY_MIN ? cfg::BRIGHT_DAY_MIN : (v > cfg::BRIGHT_DAY_MAX ? cfg::BRIGHT_DAY_MAX : v));
  } else {
    // ab 5 % abwärts in 1er Schritten, darüber in 5er Schritten
    const int dir = code / 2;
    const bool fine = dir < 0 ? u.brightNight <= cfg::BRIGHT_NIGHT_FINE_BELOW : u.brightNight < cfg::BRIGHT_NIGHT_FINE_BELOW;
    const int v = u.brightNight + dir * (fine ? 1 : cfg::BRIGHT_NIGHT_STEP);
    u.brightNight =
        static_cast<uint8_t>(v < cfg::BRIGHT_NIGHT_MIN ? cfg::BRIGHT_NIGHT_MIN : (v > cfg::BRIGHT_NIGHT_MAX ? cfg::BRIGHT_NIGHT_MAX : v));
  }
  uiprefs::save();
  ui::applyBrightness();
}

void onFlip(lv_event_t*) {
  UiSettings& u = uiprefs::get();
  u.flip180 = !u.flip180;
  uiprefs::save();
  display::setFlipped(u.flip180);
  touch::setFlipped(u.flip180);
  openBrightness();
}

void stepRow(lv_obj_t* card, const char* key, int idx, int code) {
  lv_obj_t* r = flexRow(card, 8);
  lv_obj_t* k = theme::label(r, &font_m14, false, key);
  lv_obj_set_flex_grow(k, 1);
  button(r, "\xE2\x80\x93", STEP_BTN, STEP_BTN - 8, onBrightStep, reinterpret_cast<void*>(static_cast<intptr_t>(-code)));
  dlgValue[idx] = theme::label(r, &font_m20, false, "");
  lv_obj_set_width(dlgValue[idx], 64);
  lv_obj_set_style_text_align(dlgValue[idx], LV_TEXT_ALIGN_CENTER, 0);
  button(r, "+", STEP_BTN, STEP_BTN - 8, onBrightStep, reinterpret_cast<void*>(static_cast<intptr_t>(code)));
}

void openBrightness() {
  lv_obj_t* card = subDialog("Helligkeit", Shown::Brightness);
  const UiSettings& u = uiprefs::get();
  lv_obj_t* chips = flexRow(card, 6);
  chip(chips, "Tag", u.dayNight == 0, true, onBrightMode, 0);
  chip(chips, "Nacht", u.dayNight == 1, true, onBrightMode, 1);
  chip(chips, "Auto (GPS)", u.dayNight == 2, snap.hasGps, onBrightMode, 2);  // ohne Uhr kein Auto (A2 Nr. 5)
  stepRow(card, "Tag", 0, 1);
  stepRow(card, "Nacht", 1, 2);
  // Einbaulage: Bild und Touch um 180° drehen, bleibt gespeichert
  {
    lv_obj_t* v = nullptr;
    lv_obj_t* r = row(card, "Bild um 180° drehen", &v, LV_PCT(100));
    pressable(r);
    lv_obj_add_event_cb(r, onFlip, LV_EVENT_CLICKED, nullptr);
    lv_label_set_text(v, u.flip180 ? "an" : "aus");
    lv_obj_set_style_text_color(v, theme::c(theme::ACCENT), 0);
  }
  lv_obj_t* note = theme::label(card, &font_m12, true,
                                snap.hasGps ? "Auto wechselt mit dem Sonnenstand am aktuellen Ort."
                                            : "Auto (Sonnenstand) gibt es nur mit GPS-Modul.");
  lv_obj_set_width(note, LV_PCT(100));
  lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
}

// ---------- Spar-Ziel (U Menü, Z 13) ----------
void openGoal();

void sendGoal() { ui::sendGoal(); }

void onGoalMode(lv_event_t* e) {
  uiprefs::get().goalMode = static_cast<uint8_t>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  uiprefs::save();
  sendGoal();
  openGoal();
}

void onGoalStep(lv_event_t* e) {
  const int tenths = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  UiSettings& u = uiprefs::get();
  // − oder + bei Auto macht daraus ein festes Ziel (U Menü)
  float base = u.goalFix;
  if (u.goalMode == 1) {
    const float g = snap.goalL100.get(snap.now);
    base = std::isnan(g) ? cfg::GOAL_FIX_DEFAULT : g;
  }
  float v = std::round(base * 10.0f + tenths) / 10.0f;
  v = v < cfg::GOAL_FIX_MIN ? cfg::GOAL_FIX_MIN : (v > cfg::GOAL_FIX_MAX ? cfg::GOAL_FIX_MAX : v);
  const bool wasAuto = u.goalMode != 2;
  u.goalFix = v;
  u.goalMode = 2;
  uiprefs::save();
  sendGoal();
  if (wasAuto) openGoal();
}

void openGoal() {
  lv_obj_t* card = subDialog("Spar-Ziel", Shown::Goal);
  const UiSettings& u = uiprefs::get();
  lv_obj_t* chips = flexRow(card, 6);
  chip(chips, "Aus", u.goalMode == 0, true, onGoalMode, 0);
  chip(chips, "Auto", u.goalMode == 1, true, onGoalMode, 1);
  chip(chips, "Fest", u.goalMode == 2, true, onGoalMode, 2);
  if (u.goalMode != 0) {
    lv_obj_t* r = flexRow(card, 6);
    button(r, "\xE2\x80\x93" "0,5", 46, 32, onGoalStep, reinterpret_cast<void*>(static_cast<intptr_t>(-5)));
    button(r, "\xE2\x80\x93", 36, 32, onGoalStep, reinterpret_cast<void*>(static_cast<intptr_t>(-1)));
    dlgValue[0] = theme::label(r, &font_m20, false, "");
    lv_obj_set_width(dlgValue[0], 60);
    lv_obj_set_style_text_align(dlgValue[0], LV_TEXT_ALIGN_CENTER, 0);
    button(r, "+", 36, 32, onGoalStep, reinterpret_cast<void*>(static_cast<intptr_t>(1)));
    button(r, "+0,5", 46, 32, onGoalStep, reinterpret_cast<void*>(static_cast<intptr_t>(5)));
  }
  dlgValue[1] = theme::label(card, &font_m12, true, "");
  lv_obj_set_width(dlgValue[1], LV_PCT(100));
  lv_label_set_long_mode(dlgValue[1], LV_LABEL_LONG_WRAP);
}

// ---------- Fahrzeugart (A6 Tabelle) ----------
void onBody(lv_event_t* e) {
  Command c{CmdType::SetBody};
  c.i = static_cast<int32_t>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  commands::toCalc(c);
  open();
}

void openBody() {
  lv_obj_t* card = subDialog("Fahrzeugart", Shown::Body);
  lv_obj_set_style_pad_row(card, 0, 0);
  lv_obj_set_style_margin_bottom(lv_obj_get_child(card, 0), 6, 0);
  for (uint8_t i = 0; i < cfg::BODY_TYPE_COUNT; i++) {
    lv_obj_t* v = nullptr;
    lv_obj_t* r = row(card, cfg::BODY_TYPES[i].name, &v, LV_PCT(100));
    pressable(r);
    lv_obj_add_event_cb(r, onBody, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
    char t[16];
    fmt::number(t, sizeof(t), cfg::BODY_TYPES[i].massKg, 0);
    char m[24];
    snprintf(m, sizeof(m), "%s kg", t);
    lv_label_set_text(v, m);
    if (i == snap.profile.body) lv_obj_set_style_text_color(lv_obj_get_child(r, 0), theme::c(theme::ACCENT), 0);
  }
  lv_obj_t* note = theme::label(card, &font_m12, true,
                                "Gewicht mit Fahrer und Luftwiderstand für Leistung, Bremsenergie und Eco-Score.");
  lv_obj_set_width(note, LV_PCT(100));
  lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_margin_top(note, 4, 0);
}

// ---------- Wartung (Z 9) ----------
void openMaintenance();
void maintBack() { openMaintenance(); }

void onOdo(lv_event_t*) {
  numpad::open("Tachostand eintragen", "km", snap.odoKm.get(snap.now), 7,
               [](float km) {
                 Command c{CmdType::SetOdo};
                 c.f = km;
                 commands::toCalc(c);
               },
               maintBack);
}

void onMaintDone(lv_event_t* e) {
  Command c{CmdType::MaintDone};
  c.i = static_cast<int32_t>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  commands::toCalc(c);
}

int intervalWhich = 0;
void onInterval(lv_event_t* e) {
  intervalWhich = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  numpad::open(intervalWhich == 0 ? "Ölwechsel alle" : "Inspektion alle", "km",
               intervalWhich == 0 ? snap.maint.oilIntervalKm : snap.maint.inspIntervalKm, 6,
               [](float km) {
                 Command c{CmdType::SetInterval};
                 c.i = intervalWhich;
                 c.f = km;
                 commands::toCalc(c);
               },
               maintBack);
}

void maintRow(lv_obj_t* card, const char* key, int which) {
  lv_obj_t* r = flexRow(card, 6);
  lv_obj_t* col = lv_obj_create(r);
  lv_obj_remove_style_all(col);
  lv_obj_set_size(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(col, 1);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  pressable(col);
  lv_obj_add_event_cb(col, onInterval, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(which)));
  theme::label(col, &font_m14, false, key);
  dlgValue[1 + which] = theme::label(col, &font_m12, true, "");
  button(r, "Erledigt", 72, 30, onMaintDone, reinterpret_cast<void*>(static_cast<intptr_t>(which)), true);
}

void openMaintenance() {
  lv_obj_t* card = subDialog("Wartung", Shown::Maintenance);
  lv_obj_t* v = nullptr;
  lv_obj_t* r = row(card, "Tachostand", &v, LV_PCT(100));
  pressable(r);
  lv_obj_add_event_cb(r, onOdo, LV_EVENT_CLICKED, nullptr);
  lv_obj_set_style_text_color(v, theme::c(theme::ACCENT), 0);
  dlgValue[0] = v;
  maintRow(card, "Ölwechsel", 0);
  maintRow(card, "Inspektion", 1);
}

void maintText(char* out, size_t size, float left, float interval) {
  char a[16], b[16];
  fmt::number(a, sizeof(a), std::fabs(left), 0);
  fmt::number(b, sizeof(b), interval, 0);
  if (std::isnan(left))
    snprintf(out, size, "alle %s km", b);
  else if (left <= 0)
    snprintf(out, size, "überfällig seit %s km " "\xC2\xB7" " alle %s km", a, b);
  else
    snprintf(out, size, "fällig in %s km " "\xC2\xB7" " alle %s km", a, b);
}

// ---------- Mittelwerte zurücksetzen (Diagnose) ----------
void onResetAvg(lv_event_t* e) {
  Command c{CmdType::ResetAvg};
  c.i = static_cast<int32_t>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  commands::toCalc(c);
  openDiagnose();
}

void openResetAvg() {
  lv_obj_t* card = overlay::open("Mittelwerte zurücksetzen", true, theme::DIALOG_INSET_SUB);
  shown = Shown::None;
  overlay::addDoneButton(card, [](lv_event_t*) { openDiagnose(); });
  lv_obj_set_style_pad_row(card, 8, 0);
  lv_obj_t* r = flexRow(card, 6);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_row(r, 6, 0);
  static const char* const NAMES[] = {SYM_AVG " 1 km", SYM_AVG " 10 km", SYM_AVG " 100 km", SYM_AVG " Tank", "Alle"};
  static const intptr_t MASKS[] = {1, 2, 4, 8, 15};
  for (int i = 0; i < 5; i++) button(r, NAMES[i], 84, 32, onResetAvg, reinterpret_cast<void*>(MASKS[i]));
  lv_obj_t* note = theme::label(card, &font_m12, true,
                                "Setzt nur den gewählten Schnitt auf null. Fahrten, Tankfüllungen und Kalibrierung bleiben.");
  lv_obj_set_width(note, LV_PCT(100));
  lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
}

// ---------- Menü-Aktionen ----------
void onMenuRow(lv_event_t* e) {
  const int r = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  UiSettings& u = uiprefs::get();
  switch (r) {
    case M_BRIGHT: openBrightness(); break;
    case M_GOAL: openGoal(); break;
    case M_BODY: vehicledlg::openEditor(snap); break;
    case M_MAINT: openMaintenance(); break;
    case M_DIAG: openDiagnose(); break;
    case M_REFUEL: tankdlg::openManual(snap); break;
    case M_TILES: {
      const UiSettings def;
      memcpy(u.tiles, def.tiles, sizeof(u.tiles));
      uiprefs::save();
      overlay::close();
      break;
    }
    case M_TIPS:
      u.tips = !u.tips;
      uiprefs::save();
      break;
    case M_COLD: {
      // 2000 … 3000 in 250er Schritten, danach wieder von vorn (U Menü)
      uint16_t v = snap.profile.coldRpmLimit + COLD_RPM_STEP;
      if (v > COLD_RPM_MAX || v < COLD_RPM_MIN) v = COLD_RPM_MIN;
      Command c{CmdType::SetColdRpm};
      c.i = v;
      commands::toCalc(c);
      break;
    }
    case M_SPRINT:
      u.autoSprint = !u.autoSprint;
      uiprefs::save();
      break;
    case M_ENDTRIP: {
      Command c{CmdType::EndTrip};
      commands::toCalc(c);
      overlay::close();
      break;
    }
    case M_INFO:
      overlay::close();
      ui::showInfoPage();
      break;
    default: break;
  }
}

void onDone(lv_event_t*) { overlay::close(); }
void onBackToMenu(lv_event_t*) { open(); }
void onVehicleRow(lv_event_t*) { vehicledlg::openChooser(); }
void onResetRow(lv_event_t*) { openResetAvg(); }
void onImuRow(lv_event_t*) { sensors::requestRelearn(); }
void onExportRow(lv_event_t*) { storage::requestExport(); }

}  // namespace

// Hauptmenü (7.10.2026 neu, Jos Wunsch): große Zeilen mit Symbol, eine scrollbare Liste, oben das Häufige,
// unten das Seltene. Schalter (Spartipps, Auto-Sprint) zeigen AN/AUS als Feld rechts.
void open() {
  lv_obj_t* card = overlay::open("Menü", true, theme::DIALOG_INSET_SUB);
  shown = Shown::Menu;
  shownGen = overlay::generation();
  overlay::addDoneButton(card, onDone);
  memset(menuShown, 0, sizeof(menuShown));
  lv_obj_set_style_pad_row(card, 0, 0);

  lv_obj_t* list = lv_obj_create(card);
  lv_obj_remove_style_all(list);
  lv_obj_set_width(list, LV_PCT(100));
  lv_obj_set_flex_grow(list, 1);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_ACTIVE);
  for (int k = 0; k < M_COUNT; k++) {
    const int r = MENU_ORDER[k];
    if (k == FREQUENT_COUNT) {
      lv_obj_t* h = theme::label(list, &font_m12, true, "Selten");
      lv_obj_set_style_pad_top(h, 10, 0);
      lv_obj_set_style_pad_bottom(h, 2, 0);
    }
    lv_obj_t* rr = lv_obj_create(list);
    lv_obj_remove_style_all(rr);
    lv_obj_set_size(rr, LV_PCT(100), MENU_ROW_H);
    lv_obj_set_style_border_side(rr, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(rr, 1, 0);
    lv_obj_set_style_border_color(rr, theme::c(theme::LINE), 0);
    lv_obj_set_flex_flow(rr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(rr, 10, 0);
    lv_obj_set_style_pad_hor(rr, 4, 0);
    lv_obj_remove_flag(rr, LV_OBJ_FLAG_SCROLLABLE);
    pressable(rr);
    lv_obj_add_event_cb(rr, onMenuRow, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(r)));
    lv_obj_t* ic = theme::label(rr, &font_v24, false, MENU_ICONS[r]);
    lv_obj_set_width(ic, 30);
    lv_obj_set_style_text_align(ic, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(ic, theme::c(theme::ACCENT), 0);
    lv_obj_t* key = theme::label(rr, &font_m20, false, MENU_KEYS[r]);
    lv_obj_set_flex_grow(key, 1);
    const bool toggle = r == M_TIPS || r == M_SPRINT;
    if (toggle) {
      lv_obj_t* pill = lv_obj_create(rr);
      lv_obj_remove_style_all(pill);
      lv_obj_set_size(pill, 54, 28);
      lv_obj_set_style_radius(pill, 14, 0);
      lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
      lv_obj_remove_flag(pill, LV_OBJ_FLAG_CLICKABLE);
      menuValues[r] = theme::label(pill, &font_m14, false, "");
      lv_obj_center(menuValues[r]);
      menuPills[r] = pill;
    } else {
      menuValues[r] = theme::label(rr, &font_m14, true, "");
      menuPills[r] = nullptr;
    }
  }
}

void openDiagnose() {
  lv_obj_t* card = overlay::open("Diagnose", true, theme::DIALOG_INSET_SUB);
  shown = Shown::Diagnose;
  shownGen = overlay::generation();
  overlay::addDoneButton(card, onBackToMenu);
  memset(diagShown, 0, sizeof(diagShown));

  static const char* const KEYS[ROW_COUNT] = {"Fahrzeug", "Adapter", "Protokoll", "VIN", "Abfragen", "Unterstützte PIDs",
                                              "Verbrauch aus", "Schub", "Fahrzeugart", "Mittelwerte", "Sensoren",
                                              "Sensor neu einlernen", "Fahrten exportieren"};
  lv_obj_set_style_pad_row(card, 0, 0);
  lv_obj_t* title = lv_obj_get_child(card, 0);
  lv_obj_set_style_margin_bottom(title, 10, 0);
  // Mehr Zeilen als Platz: die Karte lässt sich rollen
  lv_obj_add_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_AUTO);
  for (int i = 0; i < ROW_COUNT; i++) {
    // Ohne Sensor bzw. ohne microSD fehlen die zugehörigen Zeilen (M Optionale Sensoren)
    if ((i == ROW_IMU && !snap.hasImu) || (i == ROW_EXPORT && !snap.hasSd)) {
      diagValues[i] = nullptr;
      continue;
    }
    lv_obj_t* r = row(card, KEYS[i], &diagValues[i], LV_PCT(100));
    lv_obj_set_width(diagValues[i], DIAG_VALUE_W);
    lv_obj_set_style_text_align(diagValues[i], LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(diagValues[i], LV_LABEL_LONG_WRAP);
    if (i == ROW_VEHICLE || i == ROW_RESET || i == ROW_IMU || i == ROW_EXPORT) {
      // Profil wechseln oder neu anlegen, Mittelwerte zurücksetzen, Sensor einlernen, Export
      // (U Menü: Seltenes im Diagnose-Dialog)
      pressable(r);
      lv_event_cb_t cb = i == ROW_VEHICLE ? onVehicleRow : i == ROW_RESET ? onResetRow : i == ROW_IMU ? onImuRow : onExportRow;
      lv_obj_add_event_cb(r, cb, LV_EVENT_CLICKED, nullptr);
      lv_obj_set_style_text_color(diagValues[i], theme::c(theme::ACCENT), 0);
    }
  }
}

void update(const CarSnapshot& s) {
  snap = s;
  char text[64];
  if (stillOpen(Shown::Menu)) {
    const UiSettings& u = uiprefs::get();
    char n[16];
    // Helligkeit
    if (u.dayNight == 2)
      snprintf(text, sizeof(text), "Auto");
    else
      snprintf(text, sizeof(text), "%s %u %%", u.dayNight ? "Nacht" : "Tag", (unsigned)(u.dayNight ? u.brightNight : u.brightDay));
    setText(menuValues[M_BRIGHT], menuShown[M_BRIGHT], sizeof(menuShown[0]), text);
    // Spar-Ziel
    fmt::number(n, sizeof(n), s.goalL100.get(s.now), 1);
    if (u.goalMode == 0) snprintf(text, sizeof(text), "aus");
    else snprintf(text, sizeof(text), "%s%s", u.goalMode == 1 ? "auto " : "", n);
    setText(menuValues[M_GOAL], menuShown[M_GOAL], sizeof(menuShown[0]), text);
    fmt::number(n, sizeof(n), s.profile.displacementL, 1);
    snprintf(text, sizeof(text), "%s l " "\xC2\xB7" " %s", n, s.profile.diesel ? "Diesel" : "Benzin");
    setText(menuValues[M_BODY], menuShown[M_BODY], sizeof(menuShown[0]), text);
    // Wartung: nächster Termin
    const float oil = s.oilLeftKm.get(s.now), insp = s.inspLeftKm.get(s.now);
    if (std::isnan(oil)) {
      snprintf(text, sizeof(text), "eintragen");
    } else {
      const bool oilFirst = std::isnan(insp) || oil <= insp;
      fmt::number(n, sizeof(n), oilFirst ? oil : insp, 0);
      snprintf(text, sizeof(text), "%s in %s km", oilFirst ? "Öl" : "Insp.", n);
    }
    setText(menuValues[M_MAINT], menuShown[M_MAINT], sizeof(menuShown[0]), text);
    const bool maintWarn = (!std::isnan(oil) && oil < cfg::MAINT_WARN_KM) || (!std::isnan(insp) && insp < cfg::MAINT_WARN_KM);
    lv_obj_set_style_text_color(menuValues[M_MAINT], theme::c(maintWarn ? theme::WARN : theme::MUTED), 0);
    rateText(s, text, sizeof(text), "Abfr./s");
    setText(menuValues[M_DIAG], menuShown[M_DIAG], sizeof(menuShown[0]), text);
    setText(menuValues[M_REFUEL], menuShown[M_REFUEL], sizeof(menuShown[0]), "von Hand");
    setToggle(M_TIPS, u.tips != 0);
    fmt::number(n, sizeof(n), s.profile.coldRpmLimit, 0);
    setText(menuValues[M_COLD], menuShown[M_COLD], sizeof(menuShown[0]), n);
    setToggle(M_SPRINT, u.autoSprint != 0);
    return;
  }
  if (stillOpen(Shown::Brightness)) {
    const UiSettings& u = uiprefs::get();
    snprintf(text, sizeof(text), "%u %%", (unsigned)u.brightDay);
    setText(dlgValue[0], dlgShown[0], sizeof(dlgShown[0]), text);
    snprintf(text, sizeof(text), "%u %%", (unsigned)u.brightNight);
    setText(dlgValue[1], dlgShown[1], sizeof(dlgShown[1]), text);
    return;
  }
  if (stillOpen(Shown::Goal)) {
    const UiSettings& u = uiprefs::get();
    fmt::number(text, sizeof(text), s.goalL100.get(s.now), 1);
    setText(dlgValue[0], dlgShown[0], sizeof(dlgShown[0]), text);
    if (u.goalMode == 1) {
      char a[12], b[12];
      fmt::number(a, sizeof(a), s.goalBase.get(s.now), 1);
      fmt::number(b, sizeof(b), s.goalL100.get(s.now), 1);
      snprintf(text, sizeof(text), "%s " "\xE2\x86\x92" " %s (0,5 l unter dem Schnitt der letzten Tankfüllungen)", a, b);
    } else if (u.goalMode == 2) {
      snprintf(text, sizeof(text), "Festes Ziel von 3,0 bis 7,0 l/100 km. Gilt, bis du es änderst.");
    } else {
      snprintf(text, sizeof(text), "Ohne Ziel ist der Tank-Schnitt der Bezug in der Eco-Kurve.");
    }
    setText(dlgValue[1], dlgShown[1], sizeof(dlgShown[1]), text);
    return;
  }
  if (stillOpen(Shown::Maintenance)) {
    const float odo = s.odoKm.get(s.now);
    if (std::isnan(odo)) snprintf(text, sizeof(text), "eintragen");
    else kmText(text, sizeof(text), odo);
    setText(dlgValue[0], dlgShown[0], sizeof(dlgShown[0]), text);
    const float left[2] = {s.oilLeftKm.get(s.now), s.inspLeftKm.get(s.now)};
    const float iv[2] = {s.maint.oilIntervalKm, s.maint.inspIntervalKm};
    for (int i = 0; i < 2; i++) {
      maintText(text, sizeof(text), left[i], iv[i]);
      setText(dlgValue[1 + i], dlgShown[1 + i], sizeof(dlgShown[0]), text);
      if (dlgValue[1 + i])
        lv_obj_set_style_text_color(dlgValue[1 + i],
                                    theme::c(!std::isnan(left[i]) && left[i] < cfg::MAINT_WARN_KM ? theme::WARN : theme::MUTED), 0);
    }
    return;
  }
  if (!stillOpen(Shown::Diagnose)) {
    if (!overlay::isOpen()) shown = Shown::None;
    return;
  }
  const LinkInfo& li = s.link;
  // Fahrzeugprofil (Tippen = wechseln)
  snprintf(text, sizeof(text), "%s", s.profile.id ? s.profile.name : (s.profile.asking ? "Welches?" : fmt::NO_VALUE));
  setText(diagValues[ROW_VEHICLE], diagShown[ROW_VEHICLE], sizeof(diagShown[0]), text);
  // Adapter, Protokoll
  if (li.adapter[0])
    snprintf(text, sizeof(text), "%s (BLE)", li.adapter);
  else
    snprintf(text, sizeof(text), "%s", s.link.state == LinkState::Searching ? "Suche …" : fmt::NO_VALUE);
  if (li.channel[0]) {  // Letzte Verbindung: gewählter BLE-Kanal
    const size_t n = strlen(text);
    snprintf(text + n, sizeof(text) - n, ", %s", li.channel);
  }
  setText(diagValues[ROW_ADAPTER], diagShown[ROW_ADAPTER], sizeof(diagShown[0]), text);
  setText(diagValues[ROW_PROTOCOL], diagShown[ROW_PROTOCOL], sizeof(diagShown[0]), li.protocol[0] ? li.protocol : fmt::NO_VALUE);
  setText(diagValues[ROW_VIN], diagShown[ROW_VIN], sizeof(diagShown[0]), linktext::vin(li));
  // Abfragen pro Sekunde (A7)
  rateText(s, text, sizeof(text), "pro Sekunde");
  setText(diagValues[ROW_RATE], diagShown[ROW_RATE], sizeof(diagShown[0]), text);
  // Unterstützte PIDs
  linktext::supported(li, text, sizeof(text));
  setText(diagValues[ROW_PIDS], diagShown[ROW_PIDS], sizeof(diagShown[0]), text);
  // Verbrauchsquelle mit Kalibrierfaktor fuel_cal aus dem Profil
  char cal[12];
  fmt::number(cal, sizeof(cal), s.profile.fuelCal, 2);
  if (li.supportedKnown && s.profile.id)
    snprintf(text, sizeof(text), "%s, Kalibrierung %s", linktext::fuelSource(li, s.profile.diesel), cal);
  else
    snprintf(text, sizeof(text), "%s", fmt::NO_VALUE);
  setText(diagValues[ROW_FUEL], diagShown[ROW_FUEL], sizeof(diagShown[0]), text);
  // Schub: Entscheidung mit Grund (Pedal, Gang, Lambda und ihr Alter), F7
  setText(diagValues[ROW_CUT], diagShown[ROW_CUT], sizeof(diagShown[0]), s.cutWhy[0] ? s.cutWhy : fmt::NO_VALUE);
  // Fahrzeugart aus dem Profil (Menü → Fahrzeugart)
  const cfg::BodyType& body = cfg::BODY_TYPES[s.profile.body < cfg::BODY_TYPE_COUNT ? s.profile.body : 0];
  char mass[12];
  fmt::number(mass, sizeof(mass), body.massKg, 0);
  if (s.profile.id)
    snprintf(text, sizeof(text), "%s, %s kg", body.name, mass);
  else
    snprintf(text, sizeof(text), "%s", fmt::NO_VALUE);
  setText(diagValues[ROW_BODY], diagShown[ROW_BODY], sizeof(diagShown[0]), text);
  setText(diagValues[ROW_RESET], diagShown[ROW_RESET], sizeof(diagShown[0]), "zurücksetzen");
  // Optionale Sensoren
  {
    char g[32];
    if (s.hasGps)
      snprintf(g, sizeof(g), "GPS %s (%u Sat.)", s.gpsFix ? "Fix" : "ohne Fix", (unsigned)s.gpsSats);
    else
      snprintf(g, sizeof(g), "kein GPS");
    snprintf(text, sizeof(text), "%s, %s%s", s.hasImu ? (s.imuReady ? "MPU6050 bereit" : "MPU6050 lernt") : "kein MPU6050", g,
             s.hasSd ? ", microSD" : "");
    setText(diagValues[ROW_SENSORS], diagShown[ROW_SENSORS], sizeof(diagShown[0]), text);
  }
  setText(diagValues[ROW_IMU], diagShown[ROW_IMU], sizeof(diagShown[0]), s.imuReady ? "neu lernen" : "lernt …");
  setText(diagValues[ROW_EXPORT], diagShown[ROW_EXPORT], sizeof(diagShown[0]), s.exportMsg[0] ? s.exportMsg : "CSV auf microSD");
}

}  // namespace menu
