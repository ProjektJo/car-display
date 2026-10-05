#include "start_screen.h"

#include <cstdio>
#include <cstring>

#include "config.h"
#include "ui/theme.h"
#include "util/link_text.h"

namespace startscreen {

namespace {

// ANNAHME: eigenes Layout (die Vorschau hat keinen Startbildschirm), schlicht im Stil der Seiten
constexpr int32_t LEFT = 36;
constexpr int32_t STEPS_TOP = 48;
constexpr int32_t STEP_H = 30;
constexpr int32_t DOT = 8;
constexpr int32_t TEXT_X = LEFT + DOT + 12;
constexpr int32_t TEXT_W = 320 - TEXT_X - 16;
constexpr int32_t HINT_GAP = 8;   // Lösungssatz direkt unter dem letzten sichtbaren Schritt
constexpr int32_t RETRY_GAP = 6;
constexpr int32_t FOOT_BOTTOM = 10;

lv_obj_t* root = nullptr;
lv_obj_t* dots[linktext::STEP_COUNT];
lv_obj_t* texts[linktext::STEP_COUNT];
lv_obj_t* info;  // Lösungssatz und "Neuer Versuch in …" untereinander
lv_obj_t* hintLbl;
lv_obj_t* retryLbl;
int shownSteps = -1;
linktext::Step shown[linktext::STEP_COUNT];
char shownHint[64] = "";
char shownRetry[32] = "";
bool hiddenForGood = false;
uint32_t runningSince = 0;

void onTap(lv_event_t*) { hide(); }

void setText(lv_obj_t* l, char* shownText, size_t size, const char* text) {
  if (strcmp(shownText, text) == 0) return;
  snprintf(shownText, size, "%s", text);
  lv_label_set_text(l, text);
}

uint32_t dotColor(linktext::StepKind k) {
  switch (k) {
    case linktext::StepKind::Done: return theme::GOOD;
    case linktext::StepKind::Failed: return theme::WARN;
    default: return theme::ACCENT;
  }
}

}  // namespace

void create(lv_obj_t* parent, lv_event_cb_t onLongPress) {
  root = lv_obj_create(parent);
  lv_obj_remove_style_all(root);
  lv_obj_set_size(root, BOARD_LCD_HOR_RES, BOARD_LCD_VER_RES);
  lv_obj_set_pos(root, 0, 0);
  lv_obj_set_style_bg_color(root, theme::c(theme::BG), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(root, onTap, LV_EVENT_SHORT_CLICKED, nullptr);
  if (onLongPress) lv_obj_add_event_cb(root, onLongPress, LV_EVENT_LONG_PRESSED, nullptr);

  for (int i = 0; i < linktext::STEP_COUNT; i++) {
    const int32_t y = STEPS_TOP + i * STEP_H;
    dots[i] = lv_obj_create(root);
    lv_obj_remove_style_all(dots[i]);
    lv_obj_set_size(dots[i], DOT, DOT);
    lv_obj_set_pos(dots[i], LEFT, y + 5);
    lv_obj_set_style_radius(dots[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dots[i], LV_OPA_COVER, 0);
    lv_obj_remove_flag(dots[i], LV_OBJ_FLAG_CLICKABLE);
    texts[i] = theme::label(root, &font_m14, false);
    lv_obj_set_pos(texts[i], TEXT_X, y);
    lv_obj_set_width(texts[i], TEXT_W);
    lv_label_set_long_mode(texts[i], LV_LABEL_LONG_DOT);
    lv_obj_add_flag(dots[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(texts[i], LV_OBJ_FLAG_HIDDEN);
    shown[i].kind = linktext::StepKind::Hidden;
  }
  info = lv_obj_create(root);
  lv_obj_remove_style_all(info);
  lv_obj_set_size(info, TEXT_W, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(info, RETRY_GAP, 0);
  lv_obj_remove_flag(info, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(info, LV_OBJ_FLAG_SCROLLABLE);
  hintLbl = theme::label(info, &font_m14, false);
  lv_obj_set_width(hintLbl, TEXT_W);
  lv_label_set_long_mode(hintLbl, LV_LABEL_LONG_WRAP);
  retryLbl = theme::label(info, &font_m12, true);

  lv_obj_t* foot = theme::label(root, &font_m12, true, "Tippen zeigt die Seiten");
  lv_obj_align(foot, LV_ALIGN_BOTTOM_MID, 0, -FOOT_BOTTOM);
}

void update(const CarSnapshot& s) {
  if (!root || hiddenForGood) return;

  // Nach der ersten Verbindung mit erkanntem Fahrzeug noch kurz stehen lassen, dann für immer ausblenden
  if (s.link.state == LinkState::Running && s.link.vehicle[0]) {
    if (runningSince == 0) runningSince = s.now ? s.now : 1;
    if (s.now - runningSince >= cfg::START_SCREEN_HOLD_MS) {
      hide();
      return;
    }
  }

  linktext::Step steps[linktext::STEP_COUNT];
  linktext::startSteps(s.link, s.profile.asking, steps);
  for (int i = 0; i < linktext::STEP_COUNT; i++) {
    const linktext::Step& st = steps[i];
    if (st.kind == shown[i].kind && strcmp(st.text, shown[i].text) == 0) continue;
    shown[i] = st;
    const bool vis = st.kind != linktext::StepKind::Hidden;
    if (vis) {
      lv_obj_remove_flag(dots[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(texts[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_bg_color(dots[i], theme::c(dotColor(st.kind)), 0);
      lv_label_set_text(texts[i], st.text);
      lv_obj_set_style_text_color(texts[i], theme::c(st.kind == linktext::StepKind::Failed ? theme::WARN : theme::TEXT), 0);
    } else {
      lv_obj_add_flag(dots[i], LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(texts[i], LV_OBJ_FLAG_HIDDEN);
    }
  }

  // Satz mit der Lösung und Zeit bis zum nächsten Versuch, direkt unter dem letzten Schritt
  int visibleSteps = 0;
  for (const auto& st : steps)
    if (st.kind != linktext::StepKind::Hidden) visibleSteps++;
  if (visibleSteps != shownSteps) {
    shownSteps = visibleSteps;
    lv_obj_set_pos(info, TEXT_X, STEPS_TOP + visibleSteps * STEP_H + HINT_GAP);
  }
  const bool waiting = s.link.state == LinkState::Waiting;
  setText(hintLbl, shownHint, sizeof(shownHint), waiting ? linktext::hint(s.link.error) : "");
  char retry[32] = "";
  if (waiting && s.link.retryInS > 0) snprintf(retry, sizeof(retry), "Neuer Versuch in %u s", (unsigned)s.link.retryInS);
  setText(retryLbl, shownRetry, sizeof(shownRetry), retry);
}

bool visible() { return root && !hiddenForGood; }

void hide() {
  if (!root || hiddenForGood) return;
  hiddenForGood = true;
  lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace startscreen
