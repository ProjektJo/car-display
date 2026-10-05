#include "vehicle_dialog.h"

#include <cstdio>
#include <cstring>

#include "config.h"
#include "core/profile.h"
#include "storage/storage_task.h"
#include "ui/overlay.h"
#include "ui/symbols.h"
#include "ui/theme.h"
#include "util/format.h"

namespace vehicledlg {

void showWizard();

namespace {

// ANNAHME: eigenes Layout (die Vorschau hat keins), Zeilen wie im Menü, Knöpfe wie "Fertig"
constexpr int32_t ROW_H = 30;
constexpr int32_t ROW_PAD_HOR = 2;
constexpr int32_t BTN_H = 24;
constexpr int32_t STEP_BTN_W = 30;
constexpr int32_t VALUE_W = 64;
constexpr int32_t NAME_W = 168;
constexpr int32_t CHIP_W = 72;
constexpr int32_t ACTION_W = 96;
constexpr int32_t KB_TEXT_H = 30;

uint16_t handledAsk = 0;
uint8_t activeId = 0;
bool askingNow = false;

// Werte des Assistenten (bleiben beim Wechsel zur Tastatur erhalten)
Profile draft;

// ---------- kleine Bausteine ----------

lv_obj_t* button(lv_obj_t* parent, const char* text, int32_t w, lv_event_cb_t cb, void* user = nullptr,
                 bool accent = false) {
  lv_obj_t* b = lv_obj_create(parent);
  lv_obj_remove_style_all(b);
  lv_obj_set_size(b, w, BTN_H);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(b, theme::c(theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, theme::c(theme::LINE), LV_STATE_PRESSED);
  lv_obj_set_style_border_color(b, theme::c(accent ? theme::ACCENT : theme::LINE), 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_radius(b, theme::RADIUS_TILE, 0);
  lv_obj_t* l = theme::label(b, &font_m12, false, text);
  if (accent) lv_obj_set_style_text_color(l, theme::c(theme::ACCENT), 0);
  lv_obj_center(l);
  if (cb) {
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
    lv_obj_add_event_cb(b, cb, LV_EVENT_LONG_PRESSED_REPEAT, user);  // gedrückt halten wiederholt
  }
  return b;
}

// Zeile mit Beschriftung links und Inhalt rechts, Linie darunter
lv_obj_t* row(lv_obj_t* parent, const char* key) {
  lv_obj_t* r = lv_obj_create(parent);
  lv_obj_remove_style_all(r);
  lv_obj_set_size(r, LV_PCT(100), ROW_H);
  lv_obj_set_style_pad_hor(r, ROW_PAD_HOR, 0);
  lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(r, 1, 0);
  lv_obj_set_style_border_color(r, theme::c(theme::LINE), 0);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(r, 4, 0);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  if (key) {
    lv_obj_t* k = theme::label(r, &font_m12, false, key);
    lv_obj_set_flex_grow(k, 1);
  }
  return r;
}

bool shown = false;  // eines der Profil-Fenster ist offen

// ohne Wahl geschlossen (wirkt nur bei offener Frage)
void onDialogClosed() {
  shown = false;
  storage::dismissChoice();
}

// Fenster öffnen sich mit dieser Rückmeldung beim Schließen
void watchClose() {
  overlay::setOnClose(onDialogClosed);
  shown = true;
}

// Mit Wahl schließen: keine Rückmeldung "ohne Wahl"
void closeChosen() {
  overlay::setOnClose(nullptr);
  overlay::close();
  shown = false;
}

// Fensterwechsel innerhalb der Profil-Dialoge gilt nicht als Schließen
void switchTo(void (*openFn)()) {
  overlay::setOnClose(nullptr);
  openFn();
}

// ---------- "Welches Fahrzeug?" ----------

void onChoose(lv_event_t* e) {
  const uint8_t id = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
  closeChosen();
  storage::chooseProfile(id);
}

void onNew(lv_event_t*) { switchTo(openWizard); }
void onChooserDone(lv_event_t*) { overlay::close(); }

// ---------- Assistent ----------

lv_obj_t* nameLbl;
lv_obj_t* dispLbl;
lv_obj_t* tankLbl;
lv_obj_t* fuelBtns[2];

void showDraft() {
  char t[24], num[12];
  lv_label_set_text(nameLbl, draft.name);
  fmt::number(num, sizeof(num), draft.displacementL, 1);
  snprintf(t, sizeof(t), "%s l", num);
  lv_label_set_text(dispLbl, t);
  fmt::number(num, sizeof(num), draft.tankL, 0);
  snprintf(t, sizeof(t), "%s l", num);
  lv_label_set_text(tankLbl, t);
  for (int i = 0; i < 2; i++) {
    const bool sel = (i == 1) == (draft.fuel == FuelType::Diesel);
    lv_obj_set_style_border_color(fuelBtns[i], theme::c(sel ? theme::ACCENT : theme::LINE), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(fuelBtns[i], 0), theme::c(sel ? theme::ACCENT : theme::MUTED), 0);
  }
}

float clampStep(float v, float step, float lo, float hi) {
  v += step;
  if (v < lo) v = lo;
  if (v > hi) v = hi;
  return v;
}

enum Step : uintptr_t { DISP_DOWN, DISP_UP, TANK_DOWN, TANK_UP };
void onStep(lv_event_t* e) {
  switch (reinterpret_cast<uintptr_t>(lv_event_get_user_data(e))) {
    case DISP_DOWN: draft.displacementL = clampStep(draft.displacementL, -cfg::DISPLACEMENT_STEP_L, cfg::DISPLACEMENT_MIN_L, cfg::DISPLACEMENT_MAX_L); break;
    case DISP_UP: draft.displacementL = clampStep(draft.displacementL, cfg::DISPLACEMENT_STEP_L, cfg::DISPLACEMENT_MIN_L, cfg::DISPLACEMENT_MAX_L); break;
    case TANK_DOWN: draft.tankL = clampStep(draft.tankL, -cfg::TANK_STEP_L, cfg::TANK_MIN_L, cfg::TANK_MAX_L); break;
    case TANK_UP: draft.tankL = clampStep(draft.tankL, cfg::TANK_STEP_L, cfg::TANK_MIN_L, cfg::TANK_MAX_L); break;
  }
  // Auf eine Nachkommastelle runden, damit sich kein Rundungsfehler aufsummiert
  draft.displacementL = static_cast<int>(draft.displacementL * 10.0f + 0.5f) / 10.0f;
  showDraft();
}

void onFuel(lv_event_t* e) {
  draft.fuel = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)) ? FuelType::Diesel : FuelType::Petrol;
  showDraft();
}

void openKeyboard();
void onName(lv_event_t*) { switchTo(openKeyboard); }

void onCancel(lv_event_t*) {
  ProfileSummary list[cfg::MAX_PROFILES];
  if (storage::summaries(list, cfg::MAX_PROFILES) > 0) {
    switchTo(openChooser);  // zurück zur Liste
  } else {
    overlay::close();       // ohne Profil: wie Schließen der Frage (Standardprofil)
  }
}

void onSave(lv_event_t*) {
  draft.applyDerivedDefaults();
  closeChosen();
  storage::createProfile(draft);
}

void startDraft() {
  ProfileSummary list[cfg::MAX_PROFILES];
  const int n = storage::summaries(list, cfg::MAX_PROFILES);
  draft = Profile{};
  if (n == 0)
    snprintf(draft.name, sizeof(draft.name), "%s", cfg::DEFAULT_PROFILE_NAME);
  else
    snprintf(draft.name, sizeof(draft.name), "%s %d", cfg::NEW_PROFILE_NAME, n + 1);
}

// ---------- Tastatur für den Namen ----------

lv_obj_t* nameArea;

// Deutsche Tastatur (QWERTZ mit Umlauten); "ABC", "abc" und "1#" schalten um (lv_keyboard)
const char* const MAP_LOWER[] = {"q", "w", "e", "r", "t", "z", "u", "i", "o", "p", "ü", "\n",
                                 "a", "s", "d", "f", "g", "h", "j", "k", "l", "ö", "ä", "\n",
                                 "ABC", "y", "x", "c", "v", "b", "n", "m", "ß", LV_SYMBOL_BACKSPACE, "\n",
                                 "1#", " ", "-", LV_SYMBOL_OK, ""};
const char* const MAP_UPPER[] = {"Q", "W", "E", "R", "T", "Z", "U", "I", "O", "P", "Ü", "\n",
                                 "A", "S", "D", "F", "G", "H", "J", "K", "L", "Ö", "Ä", "\n",
                                 "abc", "Y", "X", "C", "V", "B", "N", "M", "ß", LV_SYMBOL_BACKSPACE, "\n",
                                 "1#", " ", "-", LV_SYMBOL_OK, ""};
const char* const MAP_SPECIAL[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
                                   ".", ",", "/", "(", ")", "+", "&", "'", "#", "_", "\n",
                                   "abc", "!", "?", ":", "=", "*", "%", LV_SYMBOL_BACKSPACE, "\n",
                                   "ABC", " ", "-", LV_SYMBOL_OK, ""};
// Steuerwerte je Taste: Breite (1..15) und Merkmale; in C++ als Aufzählungstyp umgewandelt
constexpr lv_buttonmatrix_ctrl_t ctrl(int v) { return static_cast<lv_buttonmatrix_ctrl_t>(v); }
constexpr lv_buttonmatrix_ctrl_t KEY = ctrl(1);
constexpr lv_buttonmatrix_ctrl_t FN = ctrl(LV_BUTTONMATRIX_CTRL_CHECKED | LV_BUTTONMATRIX_CTRL_NO_REPEAT | 2);
constexpr lv_buttonmatrix_ctrl_t SPACE = ctrl(6);
const lv_buttonmatrix_ctrl_t CTRL_TEXT[] = {KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY,
                                            KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY,
                                            FN, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, FN,
                                            FN, SPACE, KEY, FN};
const lv_buttonmatrix_ctrl_t CTRL_SPECIAL[] = {KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY,
                                               KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY,
                                               FN, KEY, KEY, KEY, KEY, KEY, KEY, FN,
                                               FN, SPACE, KEY, FN};

void onKeyboardReady(lv_event_t*) {
  const char* text = lv_textarea_get_text(nameArea);
  // Leerzeichen am Rand entfernen; leerer Name bleibt beim alten
  while (*text == ' ') text++;
  char name[sizeof(draft.name)];
  snprintf(name, sizeof(name), "%s", text);
  for (size_t n = strlen(name); n > 0 && name[n - 1] == ' '; n--) name[n - 1] = '\0';
  if (name[0]) snprintf(draft.name, sizeof(draft.name), "%s", name);
  switchTo(showWizard);
}

void openKeyboard() {
  lv_obj_t* card = overlay::open("Name", true, theme::DIALOG_INSET_SUB);
  watchClose();
  lv_obj_set_style_pad_row(card, 4, 0);
  nameArea = lv_textarea_create(card);
  lv_obj_set_size(nameArea, LV_PCT(100), KB_TEXT_H);
  lv_textarea_set_one_line(nameArea, true);
  lv_textarea_set_max_length(nameArea, cfg::PROFILE_NAME_MAX_CHARS);
  lv_textarea_set_text(nameArea, draft.name);
  lv_obj_set_style_text_font(nameArea, &font_m14, 0);
  lv_obj_set_style_text_color(nameArea, theme::c(theme::TEXT), 0);
  lv_obj_set_style_bg_color(nameArea, theme::c(theme::BG), 0);
  lv_obj_set_style_border_color(nameArea, theme::c(theme::ACCENT), 0);
  lv_obj_set_style_pad_ver(nameArea, 6, 0);
  lv_obj_add_state(nameArea, LV_STATE_FOCUSED);  // Cursor sichtbar

  lv_obj_t* kb = lv_keyboard_create(card);
  lv_obj_set_width(kb, LV_PCT(100));
  lv_obj_set_flex_grow(kb, 1);
  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_LOWER, MAP_LOWER, CTRL_TEXT);
  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_UPPER, MAP_UPPER, CTRL_TEXT);
  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_SPECIAL, MAP_SPECIAL, CTRL_SPECIAL);
  lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
  lv_keyboard_set_textarea(kb, nameArea);
  lv_obj_set_style_text_font(kb, &font_m14, LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(kb, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(kb, 0, 0);
  lv_obj_set_style_pad_gap(kb, 3, 0);
  lv_obj_set_style_bg_color(kb, theme::c(theme::LINE), LV_PART_ITEMS);
  lv_obj_set_style_bg_color(kb, theme::c(theme::BG), static_cast<lv_style_selector_t>(LV_PART_ITEMS) | LV_STATE_CHECKED);
  lv_obj_set_style_text_color(kb, theme::c(theme::TEXT), LV_PART_ITEMS);
  lv_obj_set_style_radius(kb, 4, LV_PART_ITEMS);
  lv_obj_add_event_cb(kb, onKeyboardReady, LV_EVENT_READY, nullptr);
}

}  // namespace

void openChooser() {
  ProfileSummary list[cfg::MAX_PROFILES];
  const int n = storage::summaries(list, cfg::MAX_PROFILES);
  if (n == 0) {  // noch kein Profil: gleich der Assistent
    openWizard();
    return;
  }
  lv_obj_t* card = overlay::open("Welches Fahrzeug?");
  watchClose();
  overlay::addDoneButton(card, onChooserDone);
  lv_obj_set_style_pad_row(card, 0, 0);
  lv_obj_add_flag(card, LV_OBJ_FLAG_SCROLLABLE);  // mehr Profile als Platz
  lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_margin_bottom(lv_obj_get_child(card, 0), 8, 0);

  for (int i = 0; i < n; i++) {
    lv_obj_t* r = row(card, nullptr);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(r, theme::c(theme::LINE), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(r, onChoose, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<uintptr_t>(list[i].id)));
    lv_obj_t* name = theme::label(r, &font_m14, false, list[i].name);
    lv_obj_set_flex_grow(name, 1);
    if (list[i].id == activeId && !askingNow) lv_obj_set_style_text_color(name, theme::c(theme::ACCENT), 0);
    char info[32], num[12];
    fmt::number(num, sizeof(num), list[i].displacementL, 1);
    snprintf(info, sizeof(info), "%s l " SYM_DOT " %s", num, list[i].fuel == FuelType::Diesel ? "Diesel" : "Benzin");
    theme::label(r, &font_m12, true, info);
  }
  if (n < cfg::MAX_PROFILES) {
    lv_obj_t* r = row(card, nullptr);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(r, theme::c(theme::LINE), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(r, 0, 0);
    lv_obj_add_event_cb(r, onNew, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* l = theme::label(r, &font_m14, false, "+ Neues Fahrzeug");
    lv_obj_set_style_text_color(l, theme::c(theme::ACCENT), 0);
  }
}

// Assistent mit den Werten in draft zeigen (auch nach der Tastatur)
void showWizard() {
  lv_obj_t* card = overlay::open("Neues Fahrzeug", true, theme::DIALOG_INSET_SUB);
  watchClose();
  lv_obj_set_style_pad_row(card, 0, 0);
  lv_obj_set_style_margin_bottom(lv_obj_get_child(card, 0), 6, 0);

  lv_obj_t* r = row(card, "Name");
  lv_obj_t* nameBtn = button(r, "", NAME_W, onName);
  nameLbl = lv_obj_get_child(nameBtn, 0);
  lv_obj_set_style_text_font(nameLbl, &font_m14, 0);

  r = row(card, "Kraftstoff");
  fuelBtns[0] = button(r, "Benzin", CHIP_W, onFuel, reinterpret_cast<void*>(0));
  fuelBtns[1] = button(r, "Diesel", CHIP_W, onFuel, reinterpret_cast<void*>(1));

  r = row(card, "Hubraum");
  button(r, SYM_DOWN, STEP_BTN_W, onStep, reinterpret_cast<void*>(DISP_DOWN));
  dispLbl = theme::label(r, &font_m14, false);
  lv_obj_set_width(dispLbl, VALUE_W);
  lv_obj_set_style_text_align(dispLbl, LV_TEXT_ALIGN_CENTER, 0);
  button(r, SYM_UP, STEP_BTN_W, onStep, reinterpret_cast<void*>(DISP_UP));

  r = row(card, "Tank");
  button(r, SYM_DOWN, STEP_BTN_W, onStep, reinterpret_cast<void*>(TANK_DOWN));
  tankLbl = theme::label(r, &font_m14, false);
  lv_obj_set_width(tankLbl, VALUE_W);
  lv_obj_set_style_text_align(tankLbl, LV_TEXT_ALIGN_CENTER, 0);
  button(r, SYM_UP, STEP_BTN_W, onStep, reinterpret_cast<void*>(TANK_UP));

  lv_obj_t* spacer = lv_obj_create(card);  // Knöpfe an den unteren Rand
  lv_obj_remove_style_all(spacer);
  lv_obj_set_width(spacer, LV_PCT(100));
  lv_obj_set_flex_grow(spacer, 1);

  lv_obj_t* actions = row(card, nullptr);
  lv_obj_set_style_border_width(actions, 0, 0);
  lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  button(actions, "Abbrechen", ACTION_W, onCancel);
  button(actions, "Speichern", ACTION_W, onSave, nullptr, true);
  showDraft();
}

void openWizard() {
  startDraft();
  showWizard();
}

void update(const CarSnapshot& s) {
  activeId = s.profile.id;
  askingNow = s.profile.asking;
  if (s.profile.asking && s.profile.askSeq != handledAsk) {
    handledAsk = s.profile.askSeq;
    // Ist schon eins der Profil-Fenster offen (z. B. Assistent aus dem Menü), beantwortet es die Frage;
    // nicht ersetzen, sonst gingen Eingaben verloren
    if (!shown) openChooser();
  }
}

}  // namespace vehicledlg
