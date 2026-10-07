#include "eco_popups.h"

#include <cmath>
#include <cstdio>

#include "config.h"
#include "core/commands.h"
#include "ui/overlay.h"
#include "ui/symbols.h"
#include "ui/theme.h"
#include "ui/vehicle_dialog.h"
#include "util/format.h"

namespace ecopopups {

namespace {

// Start-Karte aus der Vorschau: links/rechts 14 px, oben/unten 30 px
constexpr int32_t CARD_X = 14, CARD_Y = 30;
constexpr int32_t COLS = 4;
// Fahrzeug-Prüfung: ANNAHME: kleines Fenster mittig (die Vorschau zeigt es nicht)
constexpr int32_t CHECK_X = 20, CHECK_Y = 50;
constexpr int32_t BTN_H = 30, BTN_GAP = 8;

enum class Shown : uint8_t { None, StartCard, GearCheck, Range };
Shown shown = Shown::None;
uint32_t shownGen = 0;
uint32_t openedAt = 0;
bool startCardDone = false;
uint16_t handledCheck = 0;
bool checkAnswered = false;

bool stillOpen(Shown what) { return shown == what && overlay::isOpen() && overlay::generation() == shownGen; }

void onClose(lv_event_t*) { overlay::close(); }

lv_obj_t* cell(lv_obj_t* parent, const char* label, const char* value, uint32_t color) {
  lv_obj_t* c = lv_obj_create(parent);
  lv_obj_remove_style_all(c);
  lv_obj_set_size(c, LV_PCT(25), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_CLICKABLE);
  theme::label(c, &font_m12, true, label);
  lv_obj_t* v = theme::label(c, &font_m14, false, value);
  lv_obj_set_style_text_color(v, theme::c(color), 0);
  return c;
}

void openStartCard(const CarSnapshot& s) {
  const trip::TripRecord& t = s.lastTrip;
  lv_obj_t* card = overlay::open("");
  shown = Shown::StartCard;
  shownGen = overlay::generation();
  openedAt = s.now;
  lv_obj_set_pos(card, CARD_X, CARD_Y);
  lv_obj_set_size(card, BOARD_LCD_HOR_RES - 2 * CARD_X, BOARD_LCD_VER_RES - 2 * CARD_Y);
  // Tippen irgendwo schließt
  lv_obj_t* dim = lv_obj_get_parent(card);
  lv_obj_add_event_cb(dim, onClose, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(card, onClose, LV_EVENT_CLICKED, nullptr);

  // Kopf: "Letzte Fahrt" links, "Fahrt 12 · 23,4 km · 31 min" rechts
  lv_obj_t* head = lv_obj_create(card);
  lv_obj_remove_style_all(head);
  lv_obj_set_size(head, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_remove_flag(head, LV_OBJ_FLAG_CLICKABLE);
  theme::label(head, &font_m14, false, "Letzte Fahrt");
  char km[16], info[48];
  fmt::number(km, sizeof(km), t.km, 1);
  snprintf(info, sizeof(info), "Fahrt %u " SYM_DOT " %s km " SYM_DOT " %u min", t.number, km,
           (unsigned)std::lround(t.durationS / 60.0f));
  theme::label(head, &font_m12, true, info);

  // Ø, Kosten, Eco-Score, Gebremst
  lv_obj_t* grid = lv_obj_create(card);
  lv_obj_remove_style_all(grid);
  lv_obj_set_size(grid, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW);
  lv_obj_remove_flag(grid, LV_OBJ_FLAG_CLICKABLE);
  char v[24], n[16];
  const float avg = t.km > 0 ? t.liters / t.km * 100.0f : NAN;
  fmt::number(n, sizeof(n), avg, 1);
  snprintf(v, sizeof(v), std::isnan(avg) ? "%s" : "%s l/100", n);
  cell(grid, SYM_AVG, v, theme::TEXT);
  fmt::number(n, sizeof(n), t.cost, 2);
  snprintf(v, sizeof(v), std::isnan(t.cost) ? "%s" : "%s " SYM_EURO, n);
  cell(grid, "Kosten", v, theme::TEXT);
  fmt::number(v, sizeof(v), t.ecoScore, 0);
  const uint32_t sc = std::isnan(t.ecoScore) ? theme::TEXT
                      : t.ecoScore >= cfg::SCORE_GOOD ? theme::GOOD
                      : t.ecoScore < cfg::SCORE_OK    ? theme::WARN
                                                      : theme::TEXT;
  cell(grid, "Eco-Score", v, sc);
  fmt::number(n, sizeof(n), t.brakedL, 2);
  snprintf(v, sizeof(v), std::isnan(t.brakedL) ? "%s" : "%s l", n);
  cell(grid, "Gebremst", v, theme::TEXT);
  // Höchstens eine Hinweiszeile in warn: Wartung fällig in < 500 km (U Start-Karte); Thermostat ab Etappe 8
  const float oil = s.oilLeftKm.get(s.now), insp = s.inspLeftKm.get(s.now);
  const bool oilDue = !std::isnan(oil) && oil < cfg::MAINT_WARN_KM;
  const bool inspDue = !std::isnan(insp) && insp < cfg::MAINT_WARN_KM;
  // Thermostat geht vor (selten, aber teuer), sonst Wartung
  if (s.thermoActive) {
    lv_obj_t* line = lv_obj_create(card);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_border_side(line, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(line, 1, 0);
    lv_obj_set_style_border_color(line, theme::c(theme::LINE), 0);
    lv_obj_set_style_pad_top(line, 8, 0);
    lv_obj_set_style_margin_top(line, 6, 0);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* a = theme::label(line, &font_m12, false, "Motor wird nicht warm: Thermostat prüfen");
    lv_obj_set_style_text_color(a, theme::c(theme::WARN), 0);
  } else if (oilDue || inspDue) {
    const bool showOil = oilDue && (!inspDue || oil <= insp);
    const float left = showOil ? oil : insp;
    lv_obj_t* line = lv_obj_create(card);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_border_side(line, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(line, 1, 0);
    lv_obj_set_style_border_color(line, theme::c(theme::LINE), 0);
    lv_obj_set_style_pad_top(line, 8, 0);
    lv_obj_set_style_margin_top(line, 6, 0);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* a = theme::label(line, &font_m12, false, showOil ? "Ölwechsel fällig" : "Inspektion fällig");
    lv_obj_set_style_text_color(a, theme::c(theme::WARN), 0);
    char km[16], t[24];
    fmt::number(km, sizeof(km), left, 0);
    if (left > 0) snprintf(t, sizeof(t), "in %s km", km);
    else snprintf(t, sizeof(t), "jetzt");
    lv_obj_t* b = theme::label(line, &font_m12, false, t);
    lv_obj_set_style_text_color(b, theme::c(theme::WARN), 0);
    lv_obj_align(b, LV_ALIGN_TOP_RIGHT, 0, 0);
  }

  lv_obj_t* foot = theme::label(card, &font_m12, true, "Schließt beim Losfahren oder durch Tippen");
  lv_obj_add_flag(foot, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_align(foot, LV_ALIGN_BOTTOM_MID, 0, 0);
}

void answerCheck(bool yes) {
  checkAnswered = true;
  Command c{yes ? CmdType::GearCheckYes : CmdType::GearCheckNo};
  commands::toCalc(c);
}

void onCheckYes(lv_event_t*) {
  answerCheck(true);
  overlay::close();
}

void onCheckChange(lv_event_t*) {
  answerCheck(false);
  vehicledlg::openChooser();  // ersetzt dieses Fenster
}

// Ohne Antwort geschlossen (60 s, BOOT-Taste): Profil bleibt, nicht noch einmal fragen
void onCheckClosed() {
  if (!checkAnswered) answerCheck(false);
}

lv_obj_t* button(lv_obj_t* parent, const char* text, lv_event_cb_t cb, bool accent) {
  lv_obj_t* b = lv_obj_create(parent);
  lv_obj_remove_style_all(b);
  lv_obj_set_height(b, BTN_H);
  lv_obj_set_flex_grow(b, 1);
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
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
  return b;
}

void openGearCheck(const CarSnapshot& s) {
  lv_obj_t* card = overlay::open("");
  shown = Shown::GearCheck;
  shownGen = overlay::generation();
  checkAnswered = false;
  overlay::setOnClose(onCheckClosed);
  lv_obj_set_pos(card, CHECK_X, CHECK_Y);
  lv_obj_set_size(card, BOARD_LCD_HOR_RES - 2 * CHECK_X, BOARD_LCD_VER_RES - 2 * CHECK_Y);
  lv_obj_set_style_pad_row(card, 10, 0);
  char q[64];
  snprintf(q, sizeof(q), "Fährst du mit Fahrzeug %s?", s.profile.name);
  lv_obj_t* l = theme::label(card, &font_m14, false, q);
  lv_obj_set_width(l, LV_PCT(100));
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  theme::label(card, &font_m12, true, "Die Gänge passen nicht zum Profil.");
  lv_obj_t* row = lv_obj_create(card);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_PCT(100), BTN_H);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, BTN_GAP, 0);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(row, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, 0);
  button(row, "Ja", onCheckYes, true);
  button(row, "Fahrzeug ändern", onCheckChange, false);
}


// ---------- Reichweiten-Warnung (7.10.2026, Jos Wunsch "auffällig und realistisch") ----------
// Die Reichweite rechnet mit dem Prognose-Verbrauch (Ø 100 km, Ø 10 km, letzte Tankfüllungen), also mit der
// tatsächlichen Fahrweise. Unter 50 km und unter 20 km erscheint je einmal ein großes Fenster; wieder scharf erst
// nach dem Tanken (Reichweite wieder deutlich höher).
int rangeStage = 0;  // 0 nichts, 1 unter 50 km gewarnt, 2 unter 20 km gewarnt

void openRangeWarn(float km, bool urgent) {
  lv_obj_t* card = overlay::open("");
  shown = Shown::Range;
  shownGen = overlay::generation();
  openedAt = 0;
  lv_obj_t* dim = lv_obj_get_parent(card);
  lv_obj_add_event_cb(dim, onClose, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(card, onClose, LV_EVENT_CLICKED, nullptr);
  const uint32_t col = urgent ? theme::BAD : theme::WARN;
  lv_obj_set_style_border_color(card, theme::c(col), 0);
  lv_obj_set_style_border_width(card, 3, 0);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* row = lv_obj_create(card);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_t* icon = theme::label(row, &font_m20, false, SYM_PUMP);  // Symbole gibt es nur in font_m20
  lv_obj_set_style_text_color(icon, theme::c(col), 0);
  lv_obj_set_style_pad_bottom(icon, 6, 0);
  char n[16];
  fmt::number(n, sizeof(n), km, 0);
  lv_obj_t* v = theme::label(row, &font_m48, false, n);
  lv_obj_set_style_text_color(v, theme::c(col), 0);
  lv_obj_t* u = theme::label(row, &font_m20, true, "km");
  lv_obj_set_style_pad_bottom(u, 10, 0);
  lv_obj_t* t = theme::label(card, &font_m20, false, urgent ? "Sofort tanken!" : "Reichweite niedrig");
  lv_obj_set_style_text_color(t, theme::c(col), 0);
  theme::label(card, &font_m14, true, urgent ? "Nächste Tankstelle anfahren" : "Bald tanken");
}

void updateRange(const CarSnapshot& s) {
  const float r = s.rangeKm.get(s.now);
  if (std::isnan(r)) return;
  if (r > cfg::RANGE_LOW_KM + cfg::RANGE_REARM_KM) rangeStage = 0;  // getankt
  const int want = r < cfg::RANGE_CRIT_KM ? 2 : (r < cfg::RANGE_LOW_KM ? 1 : 0);
  if (want > rangeStage && !overlay::isOpen() && s.engineRunning()) {
    rangeStage = want;
    openedAt = s.now;
    openRangeWarn(r, want == 2);
    openedAt = s.now ? s.now : 1;
  }
  if (stillOpen(Shown::Range) && openedAt && s.now - openedAt >= cfg::RANGE_WARN_SHOW_MS) overlay::close();
}

}  // namespace

void update(const CarSnapshot& s, bool ready) {
  if (!ready) return;
  updateRange(s);

  // Start-Karte: einmal nach dem Start, wenn die letzte Fahrt mindestens 1 km lang war
  if (!startCardDone && s.profile.id && !s.profile.asking) {
    startCardDone = true;
    if (s.hasLastTrip && s.lastTrip.km >= cfg::START_CARD_MIN_KM && !overlay::isOpen()) openStartCard(s);
  }
  if (stillOpen(Shown::StartCard)) {
    const float v = s.speed.get(s.now);
    if (s.now - openedAt >= cfg::START_CARD_SHOW_MS || (!std::isnan(v) && v > cfg::START_CARD_CLOSE_KMH)) overlay::close();
  }

  // Fahrzeug-Prüfung: einmal je neuer Frage; ist gerade ein anderes Fenster offen, später
  if (s.gearCheckSeq != handledCheck && !overlay::isOpen()) {
    handledCheck = s.gearCheckSeq;
    openGearCheck(s);
  }
}

}  // namespace ecopopups
