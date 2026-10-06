// Seite "Fehlercodes" (A11, U Seite 10): Liste mit Code, Klartext und "gespeichert"/"vorläufig",
// Knöpfe "Neu lesen" und "Löschen" (nur bei stehendem Motor, mit Sicherheitsabfrage in bad).
// Darunter ggf. der Thermostat-Hinweis (Etappe 8). Positionen aus der Vorschau (pDtc).
#include <cmath>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "core/commands.h"
#include "obd/dtc.h"
#include "page.h"
#include "ui/overlay.h"
#include "ui/theme.h"

namespace {

constexpr int32_t SIDE = 8, ITEM_TOP = 8, ITEM_H = 48, ITEM_STEP = 54;
constexpr int32_t BTN_W = 146, BTN_H = 32, BTN_BOTTOM = 6;
constexpr int32_t INFO_BOTTOM = 54;
constexpr uint32_t REREAD_AFTER_MS = 60000;  // ANNAHME: beim Öffnen neu lesen, wenn älter als 60 s

lv_obj_t* button(lv_obj_t* parent, int32_t x, const char* text, lv_event_cb_t cb, void* user) {
  lv_obj_t* b = lv_obj_create(parent);
  lv_obj_remove_style_all(b);
  lv_obj_set_size(b, BTN_W, BTN_H);
  lv_obj_set_pos(b, x, theme::CONTENT_H - BTN_BOTTOM - BTN_H);
  lv_obj_set_style_radius(b, theme::RADIUS_TILE, 0);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, theme::c(theme::LINE), 0);
  lv_obj_set_style_bg_color(b, theme::c(theme::SURFACE), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(b, theme::c(theme::LINE), LV_STATE_PRESSED);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(b, cb, LV_EVENT_SHORT_CLICKED, user);
  lv_obj_t* l = theme::label(b, &font_m12, false, text);
  lv_obj_center(l);
  return b;
}

// ---------- Sicherheitsabfrage ----------
void onConfirmCancel(lv_event_t*) { overlay::close(); }
void onConfirmClear(lv_event_t*) {
  Command c{CmdType::ClearDtc};
  commands::toObd(c);
  overlay::close();
}

void openConfirm() {
  lv_obj_t* card = overlay::open("Fehlercodes löschen?");
  lv_obj_set_style_border_color(card, theme::c(theme::BAD), 0);
  lv_obj_set_style_text_color(lv_obj_get_child(card, 0), theme::c(theme::BAD), 0);
  lv_obj_t* t = theme::label(card, &font_m12, false,
                             "Die Motorkontrollleuchte geht aus und das Steuergerät setzt seine Lernwerte zurück. "
                             "Die Ursache ist damit nicht behoben.");
  lv_obj_set_width(t, LV_PCT(100));
  lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
  lv_obj_t* row = lv_obj_create(card);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_PCT(100), 30);
  lv_obj_add_flag(row, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
  for (int i = 0; i < 2; i++) {
    lv_obj_t* b = lv_obj_create(row);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 120, 30);
    lv_obj_align(b, i ? LV_ALIGN_RIGHT_MID : LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_radius(b, theme::RADIUS_TILE, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_border_color(b, theme::c(i ? theme::BAD : theme::LINE), 0);
    lv_obj_set_style_bg_color(b, theme::c(theme::SURFACE), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, theme::c(theme::LINE), LV_STATE_PRESSED);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b, i ? onConfirmClear : onConfirmCancel, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* l = theme::label(b, &font_m12, false, i ? "Löschen" : "Abbrechen");
    if (i) lv_obj_set_style_text_color(l, theme::c(theme::BAD), 0);
    lv_obj_center(l);
  }
}

struct Item {
  lv_obj_t* box = nullptr;
  lv_obj_t* code = nullptr;
  lv_obj_t* chip = nullptr;
  lv_obj_t* chipText = nullptr;
  lv_obj_t* text = nullptr;
};

class DtcPage : public Page {
 public:
  DtcPage() : Page("Fehlercodes") {}

  void create(lv_obj_t* parent) override {
    // Liste (rollt bei mehr als zwei Einträgen)
    list_ = lv_obj_create(parent);
    lv_obj_remove_style_all(list_);
    lv_obj_set_pos(list_, 0, 0);
    lv_obj_set_size(list_, BOARD_LCD_HOR_RES, theme::CONTENT_H - INFO_BOTTOM - 4);
    lv_obj_set_scroll_dir(list_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list_, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_remove_flag(list_, LV_OBJ_FLAG_CLICKABLE);
    for (int i = 0; i < 2 * DtcInfo::MAX; i++) {
      Item& it = items_[i];
      it.box = lv_obj_create(list_);
      lv_obj_remove_style_all(it.box);
      lv_obj_set_pos(it.box, SIDE, ITEM_TOP + i * ITEM_STEP);
      lv_obj_set_size(it.box, BOARD_LCD_HOR_RES - 2 * SIDE, ITEM_H);
      lv_obj_set_style_bg_color(it.box, theme::c(theme::SURFACE), 0);
      lv_obj_set_style_bg_opa(it.box, LV_OPA_COVER, 0);
      lv_obj_set_style_radius(it.box, theme::RADIUS_TILE, 0);
      lv_obj_set_style_pad_hor(it.box, 9, 0);
      lv_obj_set_style_pad_ver(it.box, 6, 0);
      lv_obj_remove_flag(it.box, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_remove_flag(it.box, LV_OBJ_FLAG_SCROLLABLE);
      it.code = theme::label(it.box, &font_m14, false, "");
      lv_obj_set_style_text_color(it.code, theme::c(theme::WARN), 0);
      it.chip = lv_obj_create(it.box);
      lv_obj_remove_style_all(it.chip);
      lv_obj_set_size(it.chip, LV_SIZE_CONTENT, 16);
      lv_obj_set_style_radius(it.chip, 8, 0);
      lv_obj_set_style_border_width(it.chip, 1, 0);
      lv_obj_set_style_border_color(it.chip, theme::c(theme::LINE), 0);
      lv_obj_set_style_pad_hor(it.chip, 6, 0);
      lv_obj_align(it.chip, LV_ALIGN_TOP_RIGHT, 0, 0);
      it.chipText = theme::label(it.chip, &font_small, true, "");
      lv_obj_center(it.chipText);
      it.text = theme::label(it.box, &font_m12, false, "");
      lv_obj_set_pos(it.text, 0, 19);
      lv_obj_set_width(it.text, BOARD_LCD_HOR_RES - 2 * SIDE - 18);
      lv_label_set_long_mode(it.text, LV_LABEL_LONG_DOT);
      lv_obj_add_flag(it.box, LV_OBJ_FLAG_HIDDEN);
    }
    empty_ = lv_obj_create(parent);
    lv_obj_remove_style_all(empty_);
    lv_obj_set_size(empty_, BOARD_LCD_HOR_RES, LV_SIZE_CONTENT);
    lv_obj_set_pos(empty_, 0, 60);
    lv_obj_set_flex_flow(empty_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(empty_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(empty_, 4, 0);
    lv_obj_remove_flag(empty_, LV_OBJ_FLAG_CLICKABLE);
    emptyTitle_ = theme::label(empty_, &font_m20, false, "");
    emptySub_ = theme::label(empty_, &font_m12, true, "");

    info_ = theme::label(parent, &font_small, true, "");
    lv_obj_set_pos(info_, SIDE, theme::CONTENT_H - INFO_BOTTOM);
    button(parent, SIDE, "Neu lesen", onRead, this);
    clearBtn_ = button(parent, BOARD_LCD_HOR_RES - SIDE - BTN_W, "Löschen", onClear, this);
  }

  void onShow() override { wantRead_ = true; }

  void update(const CarSnapshot& s) override {
    last_ = s;
    const DtcInfo& d = s.dtc;
    // Beim Öffnen lesen, wenn noch nie oder vor mehr als 60 s gelesen
    if (wantRead_) {
      wantRead_ = false;
      if (!d.known || s.now - d.readAtMs > REREAD_AFTER_MS) requestRead();
    }
    if (d.seq != shownSeq_ || d.busy != shownBusy_ || s.thermoActive != shownThermo_) {
      shownSeq_ = d.seq;
      shownBusy_ = d.busy;
      shownThermo_ = s.thermoActive;
      int n = 0;
      // Thermostat-Hinweis (A9): umrandet in warn, "Löschen" entfernt ihn nicht
      if (s.thermoActive) {
        fill(n, 0, "kein Fehlercode");
        Item& it = items_[n++];
        lv_label_set_text(it.code, "Hinweis");
        lv_label_set_text_static(it.text, "Motor wird nicht warm: Thermostat prüfen lassen");
        lv_obj_set_style_bg_opa(it.box, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(it.box, 1, 0);
        lv_obj_set_style_border_color(it.box, theme::c(theme::WARN), 0);
      }
      for (int i = n; i < 2 * DtcInfo::MAX; i++) {
        lv_obj_set_style_bg_opa(items_[i].box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(items_[i].box, 0, 0);
      }
      const int thermoRows = n;
      for (int i = 0; i < d.nStored; i++) fill(n++, d.stored[i], "gespeichert");
      for (int i = 0; i < d.nPending; i++) fill(n++, d.pending[i], "vorläufig");
      for (int i = n; i < 2 * DtcInfo::MAX; i++) lv_obj_add_flag(items_[i].box, LV_OBJ_FLAG_HIDDEN);
      count_ = n - thermoRows;
      if (n == 0) {
        lv_obj_remove_flag(empty_, LV_OBJ_FLAG_HIDDEN);
        const bool none = d.known && !d.failed;
        lv_label_set_text(emptyTitle_, d.busy ? "Lese Fehlercodes …" : none ? "Keine Fehlercodes" : "Noch nicht gelesen");
        lv_obj_set_style_text_color(emptyTitle_, theme::c(none && !d.busy ? theme::GOOD : theme::TEXT), 0);
      } else {
        lv_obj_add_flag(empty_, LV_OBJ_FLAG_HIDDEN);
      }
    }
    // "zuletzt gelesen vor … min" und Zeile mit MIL und Steuergeräten
    char t[64];
    if (d.known) {
      const uint32_t min = (s.now - d.readAtMs) / 60000;
      if (min == 0)
        snprintf(t, sizeof(t), "zuletzt gelesen gerade eben");
      else
        snprintf(t, sizeof(t), "zuletzt gelesen vor %u min", (unsigned)min);
    } else {
      snprintf(t, sizeof(t), "%s", d.failed ? "Auto antwortet nicht" : "");
    }
    setText(emptySub_, shownSub_, sizeof(shownSub_), t);
    const float mil = s.mil.get(s.now);
    snprintf(t, sizeof(t), "MIL %s" "%s%u Steuergerät%s antwortet", std::isnan(mil) ? "?" : (mil > 0.5f ? "an" : "aus"),
             " \xC2\xB7 ", (unsigned)d.ecus, d.ecus == 1 ? "" : "e");
    if (!d.known) snprintf(t, sizeof(t), "MIL %s", std::isnan(mil) ? "?" : (mil > 0.5f ? "an" : "aus"));
    setText(info_, shownInfo_, sizeof(shownInfo_), t);

    // Löschen nur bei stehendem Motor (A11)
    const bool canClear = engineStopped(s) && count_ > 0 && !d.busy;
    if (canClear != shownCanClear_) {
      shownCanClear_ = canClear;
      lv_label_set_text(lv_obj_get_child(clearBtn_, 0), engineStopped(s) ? "Löschen" : "Löschen (Motor aus)");
      lv_obj_set_style_opa(clearBtn_, canClear ? LV_OPA_COVER : LV_OPA_50, 0);
    }
  }

 private:
  static bool engineStopped(const CarSnapshot& s) {
    const float rpm = s.rpm.get(s.now);
    return !std::isnan(rpm) && rpm < 1.0f;
  }

  static void requestRead() {
    Command c{CmdType::ReadDtc};
    commands::toObd(c);
  }

  void fill(int i, uint16_t code, const char* kind) {
    if (i >= 2 * DtcInfo::MAX) return;
    Item& it = items_[i];
    char c[8];
    dtc::format(code, c, sizeof(c));
    lv_label_set_text(it.code, c);
    lv_label_set_text_static(it.chipText, kind);
    lv_label_set_text_static(it.text, dtc::text(code));
    lv_obj_remove_flag(it.box, LV_OBJ_FLAG_HIDDEN);
  }

  static void setText(lv_obj_t* l, char* shown, size_t size, const char* t) {
    if (strcmp(shown, t) == 0) return;
    snprintf(shown, size, "%s", t);
    lv_label_set_text(l, t);
  }

  static void onRead(lv_event_t*) { requestRead(); }

  static void onClear(lv_event_t* e) {
    auto* p = static_cast<DtcPage*>(lv_event_get_user_data(e));
    if (!engineStopped(p->last_) || p->count_ == 0 || overlay::isOpen()) return;
    openConfirm();
  }

  lv_obj_t* list_ = nullptr;
  Item items_[2 * DtcInfo::MAX];
  lv_obj_t* empty_ = nullptr;
  lv_obj_t* emptyTitle_ = nullptr;
  lv_obj_t* emptySub_ = nullptr;
  lv_obj_t* info_ = nullptr;
  lv_obj_t* clearBtn_ = nullptr;
  uint16_t shownSeq_ = 0xFFFF;
  bool shownBusy_ = false;
  bool shownThermo_ = false;
  bool shownCanClear_ = true;
  bool wantRead_ = false;
  int count_ = 0;
  char shownSub_[40] = "";
  char shownInfo_[64] = "";
  CarSnapshot last_;
};

}  // namespace

Page* dtcPage() {
  static DtcPage page;
  return &page;
}
