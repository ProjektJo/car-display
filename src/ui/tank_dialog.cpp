#include "tank_dialog.h"

#include <Arduino.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "calc/fuel.h"
#include "calc/trip.h"
#include "config.h"
#include "core/commands.h"
#include "hw/touch.h"
#include "ui/overlay.h"
#include "ui/symbols.h"
#include "ui/theme.h"
#include "util/format.h"

namespace tankdlg {

namespace {

// Maße aus der Vorschau (Tank-Fenster, Karte 6 px vom Rand, Innenabstand 0)
constexpr int32_t INSET = 6;
constexpr int32_t BTN_W = 32, BTN_H = 30;
constexpr int32_t UP_Y = 38, DIGIT_Y = 70, DIGIT_H = 38, DOWN_Y = 110;
constexpr int32_t LIT_X[] = {12, 48, 94};      // Zehner, Einer, Zehntel
constexpr int32_t LIT_COMMA_X = 82;
constexpr int32_t PRICE_X[] = {164, 210, 245};  // Euro, 10 ct, 1 ct
constexpr int32_t PRICE_COMMA_X = 198;
constexpr int32_t DROP_X = 130, NINE_X = 276, EURO_X = 287;
constexpr int32_t DIVIDER_X = 155;
constexpr int32_t ROW_Y = 150;
constexpr int32_t FOOT_BTN_H = 34;
constexpr int MAX_TENTHS = 999;   // 99,9 l
constexpr int MAX_CENTS = 999;    // 9,99 €
// Ziffernfeld (Vorschau): 3 × 4 Tasten 48 × 44 ab x 150
constexpr int32_t KEY_X = 150, KEY_Y = 8, KEY_W = 48, KEY_H = 44, KEY_STEP_X = 52, KEY_STEP_Y = 48;
constexpr const char* KEY_BACK = "\xEF\x95\x9A";  // U+F55A Löschtaste

enum class Field : uint8_t { None, Liters, Price, Odo };

struct State {
  int tenths = 0;         // Liter × 10
  int cents = 0;          // Euro und Cent ohne die ⁹
  bool full = true;
  bool edited = false;    // Liter von Hand geändert (Beleg)
  bool detected = false;  // über Tankanzeige erkannt
  Field keypad = Field::None;
  char entry[8] = "";     // Ziffernfeld-Eingabe
  int32_t odo = -1;       // Kilometerstand: vorbelegt mit dem berechneten, -1 = unbekannt
  bool odoEdited = false; // von Hand bestätigt bzw. korrigiert
} st;
lv_obj_t* odoLbl = nullptr;

uint32_t gen = 0;
lv_obj_t* card = nullptr;
lv_obj_t* litDigits[3] = {};
lv_obj_t* priceDigits[3] = {};
lv_obj_t* costLbl = nullptr;
lv_obj_t* srcLbl = nullptr;
lv_obj_t* fullChip = nullptr;
lv_obj_t* fullLbl = nullptr;
lv_obj_t* entryLbl = nullptr;
uint16_t handledSeq = 0;
bool seqInit = false;
// Zeitfenster (7.10.2026): erkannter Tankvorgang bzw. "Getankt?" ohne 0x2F schließen nach 15 s ohne
// Berührung von selbst = nicht getankt; das Tankmodell rechnet mit dem gespeicherten Inhalt weiter.
uint32_t timeoutGen = 0;      // Fenster mit Zeitlimit (overlay::generation), 0 = keins
uint32_t timeoutOpened = 0;   // millis beim Öffnen
lv_obj_t* timeBar = nullptr;  // schrumpfender Balken
uint16_t handledAsk = 0;
bool askInit = false;
CarSnapshot askSnap;

void build();

bool alive() { return overlay::isOpen() && overlay::generation() == gen; }

lv_obj_t* box(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}

lv_obj_t* button(lv_obj_t* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* text, lv_event_cb_t cb,
                 void* user, bool primary = false) {
  lv_obj_t* b = box(parent, x, y, w, h);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(b, theme::RADIUS_TILE, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, theme::c(primary ? theme::ACCENT : theme::LINE), 0);
  lv_obj_set_style_bg_color(b, theme::c(primary ? theme::ACCENT : theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, theme::c(theme::LINE), LV_STATE_PRESSED);
  lv_obj_t* l = theme::label(b, primary ? &font_m14 : &font_m12, false, text);
  if (primary) lv_obj_set_style_text_color(l, theme::c(theme::BG), 0);
  lv_obj_center(l);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
  lv_obj_add_event_cb(b, cb, LV_EVENT_LONG_PRESSED_REPEAT, user);  // gedrückt halten wiederholt
  return b;
}

float priceEuro() { return fuel::priceFromCents(st.cents); }

void refresh() {
  if (!alive() || st.keypad != Field::None) return;
  char t[16];
  // Liter 00,0: führende Null grau
  const int d[3] = {st.tenths / 100 % 10, st.tenths / 10 % 10, st.tenths % 10};
  for (int i = 0; i < 3; i++) {
    snprintf(t, sizeof(t), "%d", d[i]);
    lv_label_set_text(litDigits[i], t);
    lv_obj_set_style_text_color(litDigits[i], theme::c(i == 0 && d[0] == 0 ? theme::MUTED : theme::TEXT), 0);
  }
  const int p[3] = {st.cents / 100 % 10, st.cents / 10 % 10, st.cents % 10};
  for (int i = 0; i < 3; i++) {
    snprintf(t, sizeof(t), "%d", p[i]);
    lv_label_set_text(priceDigits[i], t);
  }
  char n[16];
  fmt::number(n, sizeof(n), st.tenths / 10.0f * priceEuro(), 2);
  snprintf(t, sizeof(t), "%s " SYM_EURO, n);
  lv_label_set_text(costLbl, t);
  lv_label_set_text_static(srcLbl, st.edited ? "Liter vom Beleg"
                                   : st.detected ? "Liter geschätzt über Tankanzeige"
                                                 : "Liter geschätzt aus Verbrauch");
  lv_obj_set_style_text_color(srcLbl, theme::c(st.edited ? theme::GOOD : theme::MUTED), 0);
  lv_label_set_text_static(fullLbl, st.full ? "\xEF\x80\x8C vollgetankt" : "nicht voll");  // U+F00C Haken
  const uint32_t c = st.full ? theme::GOOD : theme::MUTED;
  lv_obj_set_style_text_color(fullLbl, theme::c(c), 0);
  lv_obj_set_style_border_color(fullChip, theme::c(st.full ? theme::GOOD : theme::LINE), 0);
  // Kilometerstand: berechnet (grau) bzw. eingegeben (weiß)
  if (st.odo < 0) {
    snprintf(t, sizeof(t), "km " SYM_DASH);
  } else {
    fmt::number(n, sizeof(n), static_cast<float>(st.odo), 0);
    snprintf(t, sizeof(t), "%s km", n);
  }
  lv_label_set_text(odoLbl, t);
  lv_obj_set_style_text_color(odoLbl, theme::c(st.odoEdited ? theme::TEXT : theme::MUTED), 0);
}

// ▲/▼ je Stelle; Überlauf rechnet weiter (1,79⁹ + 10 ct = 1,89⁹)
void onLiterStep(lv_event_t* e) {
  const int step = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  st.tenths += step;
  if (st.tenths < 0) st.tenths = 0;
  if (st.tenths > MAX_TENTHS) st.tenths = MAX_TENTHS;
  st.edited = true;
  refresh();
}

void onPriceStep(lv_event_t* e) {
  const int step = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  st.cents += step;
  if (st.cents < 0) st.cents = 0;
  if (st.cents > MAX_CENTS) st.cents = MAX_CENTS;
  refresh();
}

void onFull(lv_event_t*) {
  st.full = !st.full;
  refresh();
}

void onCancel(lv_event_t*) { overlay::close(); }  // "Nicht getankt": Fehlerkennung verwerfen

void onOk(lv_event_t*) {
  const trip::FillSource src = st.edited ? trip::FillSource::Entered
                               : st.detected ? trip::FillSource::Detected
                                             : trip::FillSource::Computed;
  // Kilometerstand übernehmen, wenn er von Hand eingetragen wurde (stellt den Tachostand und die Wartung)
  if (st.odoEdited && st.odo > 0) {
    Command o{CmdType::SetOdo};
    o.f = static_cast<float>(st.odo);
    commands::toCalc(o);
  }
  Command c{CmdType::Refuel};
  c.f = st.tenths / 10.0f;
  c.f2 = priceEuro();
  c.i = (st.full ? 1 : 0) | (static_cast<int>(src) << 1);
  if (c.f > 0) commands::toCalc(c);
  overlay::close();
}

// Neu aufbauen erst nach dem Ereignis: Der gedrückte Knopf gehört selbst zum Inhalt, der gelöscht wird
void buildLater() {
  lv_async_call([](void*) { build(); }, nullptr);
}

void onDigitTap(lv_event_t* e) {
  st.keypad = static_cast<Field>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
  st.entry[0] = '\0';
  buildLater();
}

// ---------- Ziffernfeld ----------

void showEntry() {
  if (!entryLbl) return;
  char t[24];
  const bool price = st.keypad == Field::Price;
  if (st.entry[0] && st.keypad == Field::Odo)
    snprintf(t, sizeof(t), "%s", st.entry);
  else if (st.entry[0])
    snprintf(t, sizeof(t), "%s%s", st.entry, price ? SYM_NINE_SUP : "");
  else
    snprintf(t, sizeof(t), "%s", fmt::NO_VALUE);
  lv_label_set_text(entryLbl, t);
}

void onKey(lv_event_t* e) {
  const char* k = static_cast<const char*>(lv_event_get_user_data(e));
  size_t n = strlen(st.entry);
  if (strcmp(k, KEY_BACK) == 0) {
    if (n) st.entry[n - 1] = '\0';
  } else if (st.keypad == Field::Odo) {
    if (*k != ',' && n < 7) {  // ganze km, höchstens 9.999.999
      st.entry[n] = *k;
      st.entry[n + 1] = '\0';
    }
  } else if (n + 1 < sizeof(st.entry)) {
    const char* comma = strchr(st.entry, ',');
    if (*k == ',' && comma) return;
    // Preis: Euro und höchstens zwei Nachkommastellen; Liter: eine Nachkommastelle, höchstens 99,9
    const int maxDec = st.keypad == Field::Price ? 2 : 1;
    if (comma && *k != ',' && static_cast<int>(strlen(comma + 1)) >= maxDec) return;
    if (!comma && *k != ',' && static_cast<int>(n) >= (st.keypad == Field::Price ? 1 : 2)) return;
    st.entry[n] = *k;
    st.entry[n + 1] = '\0';
  }
  showEntry();
}

void onTakeOver(lv_event_t*) {
  if (st.entry[0]) {
    // Eingabe "1,79" bzw. "28,4" in ganze Hundertstel bzw. Zehntel
    int whole = 0, frac = 0, fracDigits = 0;
    bool afterComma = false;
    for (const char* p = st.entry; *p; p++) {
      if (*p == ',') {
        afterComma = true;
      } else if (afterComma) {
        frac = frac * 10 + (*p - '0');
        fracDigits++;
      } else {
        whole = whole * 10 + (*p - '0');
      }
    }
    if (st.keypad == Field::Odo) {
      st.odo = whole;
      st.odoEdited = true;
    } else if (st.keypad == Field::Price) {
      while (fracDigits < 2) {
        frac *= 10;
        fracDigits++;
      }
      st.cents = whole * 100 + frac;
      if (st.cents > MAX_CENTS) st.cents = MAX_CENTS;
    } else {
      if (fracDigits == 0) frac = 0;
      st.tenths = whole * 10 + frac;
      if (st.tenths > MAX_TENTHS) st.tenths = MAX_TENTHS;
      st.edited = true;
    }
  }
  st.keypad = Field::None;
  buildLater();
}

void buildKeypad() {
  const bool price = st.keypad == Field::Price, odo = st.keypad == Field::Odo;
  lv_obj_t* t = theme::label(card, &font_m14, false, odo ? "Kilometerstand" : (price ? "Preis pro Liter" : "Getankte Liter"));
  lv_obj_set_pos(t, 12, 8);
  entryLbl = theme::label(card, &font_m28, false, "");
  lv_obj_set_pos(entryLbl, 12, 34);
  lv_obj_t* u = theme::label(card, &font_m12, true, odo ? "km (laut Tacho)" : (price ? SYM_EURO "/l" : "l"));
  lv_obj_set_pos(u, 12, 72);
  showEntry();
  static const char* const KEYS[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", ",", "0", KEY_BACK};
  for (int i = 0; i < 12; i++) {
    lv_obj_t* b = button(card, KEY_X + (i % 3) * KEY_STEP_X, KEY_Y + (i / 3) * KEY_STEP_Y, KEY_W, KEY_H, KEYS[i], onKey,
                         const_cast<char*>(KEYS[i]));
    lv_obj_set_style_text_font(lv_obj_get_child(b, 0), &font_m20, 0);
  }
  button(card, 12, BOARD_LCD_VER_RES - 2 * INSET - 10 - 36, 120, 36, "Übernehmen", onTakeOver, nullptr, true);
}

// ---------- Hauptansicht ----------

void digitColumn(int32_t x, lv_obj_t** digit, lv_event_cb_t stepCb, int step, Field field) {
  button(card, x, UP_Y, BTN_W, BTN_H, SYM_UP, stepCb, reinterpret_cast<void*>(static_cast<intptr_t>(step)));
  lv_obj_t* d = box(card, x, DIGIT_Y, BTN_W, DIGIT_H);
  lv_obj_add_flag(d, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(d, onDigitTap, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(field)));
  *digit = theme::label(d, &font_m28, false, "0");
  lv_obj_center(*digit);
  button(card, x, DOWN_Y, BTN_W, BTN_H, SYM_DOWN, stepCb, reinterpret_cast<void*>(static_cast<intptr_t>(-step)));
}

lv_obj_t* comma(int32_t x) {
  lv_obj_t* c = theme::label(card, &font_m28, false, ",");
  lv_obj_set_pos(c, x, DIGIT_Y + 2);
  return c;
}

void buildMain() {
  lv_obj_t* title = theme::label(card, &font_m14, false, "Getankt");
  lv_obj_set_pos(title, 12, 8);
  srcLbl = theme::label(card, &font_m12, true, "");
  lv_obj_align(srcLbl, LV_ALIGN_TOP_RIGHT, -12, 10);
  lv_obj_t* l1 = theme::label(card, &font_m12, true, "Liter");
  lv_obj_set_pos(l1, 12, 24);
  lv_obj_t* l2 = theme::label(card, &font_m12, true, "Preis pro Liter");
  lv_obj_set_pos(l2, PRICE_X[0], 24);

  static const int LIT_STEP[3] = {100, 10, 1};
  static const int PRICE_STEP[3] = {100, 10, 1};
  for (int i = 0; i < 3; i++) digitColumn(LIT_X[i], &litDigits[i], onLiterStep, LIT_STEP[i], Field::Liters);
  comma(LIT_COMMA_X);
  for (int i = 0; i < 3; i++) digitColumn(PRICE_X[i], &priceDigits[i], onPriceStep, PRICE_STEP[i], Field::Price);
  comma(PRICE_COMMA_X);
  lv_obj_t* drop = theme::label(card, &font_m20, false, SYM_DROP);
  lv_obj_set_style_text_color(drop, theme::c(theme::ACCENT), 0);
  lv_obj_set_pos(drop, DROP_X, DIGIT_Y + 8);
  lv_obj_t* nine = theme::label(card, &font_m14, false, "9");
  lv_obj_set_pos(nine, NINE_X, DIGIT_Y + 2);
  lv_obj_t* euro = theme::label(card, &font_m20, false, SYM_EURO);
  lv_obj_set_style_text_color(euro, theme::c(theme::ACCENT), 0);
  lv_obj_set_pos(euro, EURO_X, DIGIT_Y + 8);
  lv_obj_t* div = box(card, DIVIDER_X, 44, 1, 92);
  lv_obj_set_style_bg_color(div, theme::c(theme::LINE), 0);
  lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);

  // Schalter vollgetankt (Chip) und Kosten
  fullChip = box(card, 12, ROW_Y, LV_SIZE_CONTENT, 22);
  lv_obj_add_flag(fullChip, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_radius(fullChip, 11, 0);
  lv_obj_set_style_border_width(fullChip, 1, 0);
  lv_obj_set_style_pad_hor(fullChip, 9, 0);
  lv_obj_add_event_cb(fullChip, onFull, LV_EVENT_CLICKED, nullptr);
  fullLbl = theme::label(fullChip, &font_m12, false, "");
  lv_obj_center(fullLbl);
  lv_obj_t* costRow = box(card, 0, ROW_Y + 2, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(costRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(costRow, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_column(costRow, 4, 0);
  theme::label(costRow, &font_m12, true, "Kosten");
  costLbl = theme::label(costRow, &font_m14, false, "");
  lv_obj_set_style_text_color(costLbl, theme::c(theme::ACCENT), 0);
  lv_obj_align(costRow, LV_ALIGN_TOP_RIGHT, -12, ROW_Y + 2);

  const int32_t footY = BOARD_LCD_VER_RES - 2 * INSET - 10 - FOOT_BTN_H;
  lv_obj_t* no = button(card, 12, footY, 96, FOOT_BTN_H, "\xEF\x80\x8D" " Nein", onCancel, nullptr);
  lv_obj_set_style_text_color(lv_obj_get_child(no, 0), theme::c(theme::MUTED), 0);
  // Kilometerstand: vorbelegt mit dem berechneten Stand, Tippen zum Bestätigen bzw. Ändern
  lv_obj_t* ob = button(card, 114, footY, 104, FOOT_BTN_H, "", onDigitTap, reinterpret_cast<void*>(static_cast<intptr_t>(Field::Odo)));
  odoLbl = lv_obj_get_child(ob, 0);
  lv_obj_set_style_text_font(odoLbl, &font_m14, 0);
  button(card, BOARD_LCD_HOR_RES - 2 * INSET - 12 - 72, footY, 72, FOOT_BTN_H, "\xEF\x80\x8C" " OK", onOk, nullptr, true);
  refresh();
}

void build() {
  if (!alive()) return;
  lv_obj_clean(card);
  entryLbl = nullptr;
  if (st.keypad == Field::None)
    buildMain();
  else
    buildKeypad();
}

void open(const CarSnapshot& s, float liters, bool detected) {
  st = State();
  st.detected = detected;
  st.tenths = std::isnan(liters) || liters < 0 ? 0 : static_cast<int>(std::lround(liters * 10));
  if (st.tenths > MAX_TENTHS) st.tenths = MAX_TENTHS;
  // Vorschlag: letzter Zapfsäulenpreis (A7)
  const float last = s.pumpPrice.get(s.now);
  st.cents = fuel::centsFromPrice(std::isnan(last) ? cfg::PRICE_DEFAULT : last);
  const float odo = s.odoKm.get(s.now);
  st.odo = std::isnan(odo) ? -1 : static_cast<int32_t>(std::lround(odo));
  card = overlay::open("", false, INSET);  // bleibt, bis es beantwortet ist
  gen = overlay::generation();
  lv_obj_set_style_border_color(card, theme::c(theme::ACCENT), 0);
  lv_obj_set_style_pad_all(card, 0, 0);
  lv_obj_set_layout(card, LV_LAYOUT_NONE);
  build();
}

}  // namespace

// ---------- "Getankt?" ohne Füllstand vom Auto ----------

void startTimeout(lv_obj_t* parent, int32_t y) {
  timeoutGen = overlay::generation();
  timeoutOpened = millis();
  timeBar = box(parent, 12, y, BOARD_LCD_HOR_RES - 2 * INSET - 24, 4);
  lv_obj_set_style_radius(timeBar, 2, 0);
  lv_obj_set_style_bg_color(timeBar, theme::c(theme::ACCENT), 0);
  lv_obj_set_style_bg_opa(timeBar, LV_OPA_COVER, 0);
}

// Vollgetankt: Liter = Tankgröße minus physischer Rest (sonst die seit dem letzten Tanken berechneten)
void onAskFull(lv_event_t*) {
  const CarSnapshot& s = askSnap;
  const float phys = s.tankPhysL.get(s.now);
  float liters = NAN;
  if (!std::isnan(phys) && !std::isnan(s.profile.tankL)) liters = s.profile.tankL - phys;
  if (!(liters > 0)) liters = s.fillL.get(s.now);
  if (!(liters > 0)) liters = std::isnan(s.profile.tankL) ? 0.0f : s.profile.tankL / 2;  // grobe Annahme
  const float price = s.pumpPrice.get(s.now);
  Command c{CmdType::Refuel};
  c.f = liters;
  c.f2 = std::isnan(price) ? cfg::PRICE_DEFAULT : price;
  c.i = 1 | (static_cast<int>(trip::FillSource::Computed) << 1);
  if (c.f > 0) commands::toCalc(c);
  overlay::close();
}

void onAskLiters(lv_event_t*) {
  timeoutGen = 0;
  const CarSnapshot s = askSnap;
  open(s, s.fillL.get(s.now), false);  // Tank-Fenster ohne Zeitlimit
}

void onAskNo(lv_event_t*) { overlay::close(); }

void openAsk(const CarSnapshot& s) {
  askSnap = s;
  card = overlay::open("", false, 20);
  gen = overlay::generation();
  lv_obj_set_style_border_color(card, theme::c(theme::ACCENT), 0);
  lv_obj_set_style_pad_all(card, 0, 0);
  lv_obj_set_layout(card, LV_LAYOUT_NONE);
  const int32_t w = BOARD_LCD_HOR_RES - 40;
  lv_obj_t* icon = theme::label(card, &font_m20, false, SYM_PUMP);
  lv_obj_set_style_text_color(icon, theme::c(theme::ACCENT), 0);
  lv_obj_set_pos(icon, 14, 14);
  lv_obj_t* t = theme::label(card, &font_m20, false, "Getankt?");
  lv_obj_set_pos(t, 44, 12);
  const float usable = s.tankL.get(s.now);
  char txt[48], n[12];
  if (std::isnan(usable)) {
    snprintf(txt, sizeof(txt), "Tankinhalt unbekannt");
  } else {
    fmt::number(n, sizeof(n), usable, 0);
    snprintf(txt, sizeof(txt), "Tank zuletzt: %s l", n);
  }
  lv_obj_t* sub = theme::label(card, &font_m14, true, txt);
  lv_obj_set_pos(sub, 14, 46);
  // drei große Knöpfe mit Symbol
  const int32_t bw = (w - 2 * 12 - 2 * 8) / 3, by = 82, bh = 64;
  struct B { const char* icon; const char* text; lv_event_cb_t cb; uint32_t col; };
  const B bs[3] = {{LV_SYMBOL_OK, "Voll", onAskFull, theme::GOOD},
                   {LV_SYMBOL_EDIT, "Liter", onAskLiters, theme::ACCENT},
                   {LV_SYMBOL_CLOSE, "Nein", onAskNo, theme::MUTED}};
  for (int i = 0; i < 3; i++) {
    lv_obj_t* b = box(card, 12 + i * (bw + 8), by, bw, bh);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(b, theme::RADIUS_TILE, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, theme::c(bs[i].col), 0);
    lv_obj_set_style_bg_color(b, theme::c(theme::SURFACE), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, theme::c(theme::LINE), LV_STATE_PRESSED);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* ic = theme::label(b, &font_v24, false, bs[i].icon);
    lv_obj_set_style_text_color(ic, theme::c(bs[i].col), 0);
    theme::label(b, &font_m14, false, bs[i].text);
    lv_obj_add_event_cb(b, bs[i].cb, LV_EVENT_CLICKED, nullptr);
  }
  lv_obj_t* hint = theme::label(card, &font_m12, true, "Ohne Antwort: nicht getankt");
  lv_obj_set_pos(hint, 14, by + bh + 10);
  startTimeout(card, by + bh + 30);
}

void openManual(const CarSnapshot& s) {
  timeoutGen = 0;
  open(s, s.fillL.get(s.now), false);
}

void update(const CarSnapshot& s) {
  if (!seqInit) {
    seqInit = true;
    handledSeq = s.refuelSeq;
  }
  if (s.refuelSeq != handledSeq) {
    handledSeq = s.refuelSeq;
    Serial.printf("Tankvorgang erkannt: %.1f l\n", s.refuelL);
    open(s, s.refuelL, true);
    startTimeout(card, BOARD_LCD_VER_RES - 2 * INSET - 6);
  }
  // Ohne 0x2F: "Getankt?" (warmer Motor und Tank höchstens halb voll, bzw. Inhalt unbekannt)
  if (!askInit) {
    askInit = true;
    handledAsk = s.refuelAskSeq;
  }
  if (s.refuelAskSeq != handledAsk) {
    handledAsk = s.refuelAskSeq;
    Serial.println("Tanken? Abfrage ohne Füllstand vom Auto");
    openAsk(s);
  }
  // Zeitlimit: Balken schrumpft; eine Berührung hebt das Limit auf; abgelaufen = nicht getankt
  if (timeoutGen && overlay::isOpen() && overlay::generation() == timeoutGen) {
    const uint32_t now = millis();
    if (touch::lastTouchMs() > timeoutOpened) {
      timeoutGen = 0;
      if (timeBar) lv_obj_add_flag(timeBar, LV_OBJ_FLAG_HIDDEN);
    } else if (now - timeoutOpened >= cfg::REFUEL_ASK_TIMEOUT_MS) {
      timeoutGen = 0;
      Serial.println("Tanken: keine Antwort, nicht getankt");
      overlay::close();
    } else if (timeBar) {
      const int32_t full = BOARD_LCD_HOR_RES - 2 * INSET - 24;
      lv_obj_set_width(timeBar, full - static_cast<int32_t>(full * (now - timeoutOpened) / cfg::REFUEL_ASK_TIMEOUT_MS));
    }
  } else if (timeoutGen) {
    timeoutGen = 0;
  }
}

}  // namespace tankdlg
