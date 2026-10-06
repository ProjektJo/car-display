#include "ui.h"

#include <Arduino.h>

#include <cmath>
#include <esp_heap_caps.h>

#include "config.h"
#include "core/car_state_store.h"
#include "core/commands.h"
#include "hw/display.h"
#include "hw/touch.h"
#include "ui/eco_popups.h"
#include "ui/history.h"
#include "calc/perf.h"
#include "ui/live.h"
#include "ui/values.h"
#include "ui/tank_dialog.h"
#include "ui/ui_prefs.h"
#include "ui/menu.h"
#include "ui/overlay.h"
#include "ui/pages/page.h"
#include "ui/start_screen.h"
#include "ui/statusbar.h"
#include "ui/theme.h"
#include "ui/vehicle_dialog.h"
#include "util/button_logic.h"

namespace ui {

namespace {

Page* const PAGES[] = {
    ecoPage(),     sportPage(),  sprintPage(),  overviewPage(), bigPage(), tripTankPage(),
    chartsPage(),  gforcePage(), historyPage(), dtcPage(),      infoPage(),
};
constexpr int PAGE_COUNT = sizeof(PAGES) / sizeof(PAGES[0]);
static_assert(PAGE_COUNT <= statusbar::MAX_PAGES, "zu viele Seiten für die Statusleiste");

lv_obj_t* containers[PAGE_COUNT];
int current = 0;  // Index in PAGES; Startseite ist Eco
constexpr int SPRINT_PAGE = 2;  // Index der Sprint-Seite in PAGES
perf::AutoSprint autoSprint;     // Auto-Sprint (A10): zur Sprint-Seite und zurück
int beforeSprint = -1;
uint32_t lastAutoBright = 0;
CarSnapshot snap;

#ifdef SIMULATE_OBD
constexpr uint32_t BUTTON_VERY_LONG_MS = cfg::BUTTON_SIM_SPRINT_MS;
#else
constexpr uint32_t BUTTON_VERY_LONG_MS = 0;
#endif
ButtonLogic bootButton(cfg::BUTTON_DEBOUNCE_MS, cfg::BUTTON_LONG_MS, BUTTON_VERY_LONG_MS);

uint32_t tickCb() { return millis(); }

void logCb(lv_log_level_t, const char* buf) { Serial.print(buf); }

// Sichtbare Seiten (z. B. G-Kraft nur mit Sensor); liefert Anzahl und Position der aktuellen
int visibleInfo(int& pos) {
  int count = 0;
  pos = 0;
  for (int i = 0; i < PAGE_COUNT; i++) {
    if (!PAGES[i]->available(snap)) continue;
    if (i == current) pos = count;
    count++;
  }
  return count;
}

void showPage(int index) {
  if (index != current) {
    lv_obj_add_flag(containers[current], LV_OBJ_FLAG_HIDDEN);
    current = index;
  }
  lv_obj_remove_flag(containers[current], LV_OBJ_FLAG_HIDDEN);
  int pos = 0;
  const int count = visibleInfo(pos);
  statusbar::setPage(PAGES[current]->name(), pos, count);
  PAGES[current]->onShow();
  PAGES[current]->update(snap);
}

// Nächste bzw. vorige sichtbare Seite, Endlosschleife (U Bedienung). Ein Seitenwechsel von Hand
// hebt den Rücksprung des Auto-Sprints auf (A10).
void stepPage(int dir) {
  autoSprint.cancel();
  int i = current;
  for (int n = 0; n < PAGE_COUNT; n++) {
    i = (i + dir + PAGE_COUNT) % PAGE_COUNT;
    if (PAGES[i]->available(snap)) break;
  }
  showPage(i);
}

void openMenu() { menu::open(); }

void longPressCb(lv_event_t*) {
  if (!overlay::isOpen()) openMenu();
}

void createUi() {
  lv_obj_t* scr = lv_screen_active();
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  for (int i = 0; i < PAGE_COUNT; i++) {
    lv_obj_t* c = lv_obj_create(scr);
    lv_obj_remove_style_all(c);
    lv_obj_set_pos(c, 0, theme::STATUSBAR_H);
    lv_obj_set_size(c, BOARD_LCD_HOR_RES, theme::CONTENT_H);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);  // langes Drücken auf eine freie Fläche = Menü
    lv_obj_add_event_cb(c, longPressCb, LV_EVENT_LONG_PRESSED, nullptr);
    lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN);
    PAGES[i]->create(c);
    containers[i] = c;
  }
  statusbar::create(scr, longPressCb);
  startscreen::create(scr, longPressCb);  // liegt über Seiten und Statusleiste, Fenster liegen darüber
}

void handleButton(uint32_t now) {
  switch (bootButton.update(digitalRead(BOARD_PIN_BOOT) == LOW, now)) {
    case ButtonEvent::Short:
      // ANNAHME: Ist ein Fenster offen, schließt ein kurzer Druck es, statt die Seite zu wechseln.
      if (overlay::isOpen())
        overlay::close();
      else if (startscreen::visible())
        startscreen::hide();
      else
        stepPage(+1);
      break;
    case ButtonEvent::Long:
      if (!overlay::isOpen()) openMenu();
      break;
    case ButtonEvent::VeryLong: {
      Command c{CmdType::SimSprint};
      commands::toObd(c);
      Serial.println("Simulator: Vollgas-Sequenz beim nächsten Halt");
      break;
    }
    case ButtonEvent::None:
      break;
  }
}

void handleSwipe() {
  touch::setSwipeEnabled(!overlay::isOpen());
  const Swipe sw = touch::takeSwipe();
  if (startscreen::visible() && (sw == Swipe::Left || sw == Swipe::Right)) {
    startscreen::hide();  // erst einmal nur den Startbildschirm weg, Seite bleibt Eco
    return;
  }
  switch (sw) {
    case Swipe::Left:  // Finger nach links = nächste Seite
      stepPage(+1);
      break;
    case Swipe::Right:
      stepPage(-1);
      break;
    case Swipe::Down:  // von oben nach unten = Menü (Alternative)
      openMenu();
      break;
    case Swipe::None:
      break;
  }
}

// Bildschirmfoto für die Fehlersuche: Bildschirm (RGB565) und darüber liegende Fenster (ARGB8888)
// als Rohdaten über USB. Kopfzeile "SHOT <Breite> <Höhe>", dann beide Bilder. tools/screenshot.py setzt sie zusammen.
void sendScreenshot() {
  constexpr uint32_t W = BOARD_LCD_HOR_RES, H = BOARD_LCD_VER_RES;
  static uint8_t* bufScreen = nullptr;
  static uint8_t* bufTop = nullptr;
  const uint32_t sizeScreen = W * H * 2 + LV_DRAW_BUF_ALIGN, sizeTop = W * H * 4 + LV_DRAW_BUF_ALIGN;
  if (!bufScreen) bufScreen = static_cast<uint8_t*>(heap_caps_malloc(sizeScreen, MALLOC_CAP_SPIRAM));
  if (!bufTop) bufTop = static_cast<uint8_t*>(heap_caps_malloc(sizeTop, MALLOC_CAP_SPIRAM));
  if (!bufScreen || !bufTop) {
    Serial.println("Bildschirmfoto: kein Speicher");
    return;
  }
  lv_draw_buf_t screen, top;
  lv_draw_buf_init(&screen, W, H, LV_COLOR_FORMAT_RGB565, 0, lv_draw_buf_align(bufScreen, LV_COLOR_FORMAT_RGB565), W * H * 2);
  lv_draw_buf_init(&top, W, H, LV_COLOR_FORMAT_ARGB8888, 0, lv_draw_buf_align(bufTop, LV_COLOR_FORMAT_ARGB8888), W * H * 4);
  if (lv_snapshot_take_to_draw_buf(lv_screen_active(), LV_COLOR_FORMAT_RGB565, &screen) != LV_RESULT_OK ||
      lv_snapshot_take_to_draw_buf(lv_layer_top(), LV_COLOR_FORMAT_ARGB8888, &top) != LV_RESULT_OK) {
    Serial.println("Bildschirmfoto: fehlgeschlagen");
    return;
  }
  Serial.printf("\nSHOT %u %u\n", (unsigned)W, (unsigned)H);
  // In Stücken schreiben und warten, bis jedes ganz raus ist (USB verwirft sonst bei vollem Puffer)
  auto writeAll = [](const uint8_t* p, size_t n) {
    while (n > 0) {
      const size_t w = Serial.write(p, n > 1024 ? 1024 : n);
      if (w == 0) vTaskDelay(1);
      p += w;
      n -= w;
    }
  };
  writeAll(screen.data, W * H * 2);
  writeAll(top.data, W * H * 4);
  Serial.flush();
}

}  // namespace

void applyBrightness() {
  const UiSettings& u = uiprefs::get();
  // Auto (GPS): Sonnenstand am aktuellen Ort; ohne GPS-Fix gilt Tag
  float f = u.dayNight == 1 ? 1.0f : 0.0f;
  if (u.dayNight == 2 && !std::isnan(snap.nightFactor)) f = snap.nightFactor;
  display::setBrightness(static_cast<uint8_t>(std::lround(u.brightDay + (u.brightNight - u.brightDay) * f)));
  theme::setNight(f >= 0.5f);
}

void sendGoal() {
  const UiSettings& u = uiprefs::get();
  Command c{CmdType::SetGoal};
  c.i = u.goalMode;
  c.f = u.goalFix;
  commands::toCalc(c);
}

void showInfoPage() {
  for (int i = 0; i < PAGE_COUNT; i++)
    if (PAGES[i] == infoPage()) showPage(i);
}

void task(void*) {
  // LVGL holt seine 256 kB aus dem PSRAM (lv_conf.h); ohne PSRAM stürzt lv_init ab
  if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) < LV_MEM_SIZE) {
    Serial.println("FEHLER: zu wenig PSRAM für LVGL. memory_type dio_opi in platformio.ini prüfen.");
  }
  Serial.println("Start: LVGL");
  lv_init();
  lv_tick_set_cb(tickCb);
  lv_log_register_print_cb(logCb);

  lv_display_t* disp = display::init();  // schaltet das Licht schon mit einer Startzeile ein
  if (!disp) {
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));  // ohne Zeichenpuffer geht nichts
  }
  theme::init(disp);
  Serial.println("Start: Touch");
  touch::init();

  Serial.println("Start: Oberfläche aufbauen");
  uiprefs::load();
  history::init();
  live::init();
  carstate::snapshot(snap);
  createUi();
  showPage(current);
  Serial.println("Start: erstes Bild zeichnen");
  lv_refr_now(disp);
  applyBrightness();
  sendGoal();
  Serial.printf("UI bereit, freier interner RAM %u kB, PSRAM %u kB\n",
                (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
                (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));

  uint32_t nextSnap = millis();
  for (;;) {
    const uint32_t now = millis();
    if (static_cast<int32_t>(now - nextSnap) >= 0) {
      nextSnap = now + cfg::UI_SNAPSHOT_PERIOD_MS;
      carstate::snapshot(snap);
      statusbar::update(snap);
      startscreen::update(snap);
      menu::update(snap);
      vehicledlg::update(snap);  // "Welches Fahrzeug?", wenn kein Profil eindeutig passt
      if (!PAGES[current]->available(snap)) stepPage(+1);  // z. B. Sensor fehlt plötzlich
      // Auto-Sprint (A10), abschaltbar im Menü
      switch (autoSprint.update(snap.now, uiprefs::get().autoSprint != 0, snap.sprint.launchSeq, snap.sprint.state, snap.sprint.doneAtMs,
                                values::value(values::Key::Pedal, snap), overlay::isOpen())) {
        case perf::AutoSprint::Action::ShowSprint:
          beforeSprint = current;
          startscreen::hide();
          showPage(SPRINT_PAGE);
          Serial.println("Auto-Sprint: Sprint-Seite");
          break;
        case perf::AutoSprint::Action::Return:
          if (beforeSprint >= 0 && PAGES[beforeSprint]->available(snap)) showPage(beforeSprint);
          Serial.println("Auto-Sprint: zurück");
          break;
        case perf::AutoSprint::Action::None:
          break;
      }
      history::tick(snap);
      live::tick(snap);
      // Helligkeit "Auto (GPS)": Sonnenstand mit 15 min Übergang (A9), einmal je Sekunde
      if (uiprefs::get().dayNight == 2 && snap.now / 1000 != lastAutoBright) {
        lastAutoBright = snap.now / 1000;
        applyBrightness();
      }
      for (Page* p : PAGES) p->tick(snap);
      PAGES[current]->update(snap);
      ecopopups::update(snap, !startscreen::visible());
      tankdlg::update(snap);  // Tank-Fenster bei erkanntem Tankvorgang, über jeder Seite
    }
    handleButton(now);
    handleSwipe();
    // Fehlersuche über USB: S = Bildschirmfoto, n/p = nächste/vorige Seite, g = Tank-Fenster, m = Menü,
    // x = Fenster schließen
    if (cfg::SCREENSHOT_SERIAL && Serial.available() > 0) {
      switch (Serial.read()) {
        case 'S': sendScreenshot(); break;
        case 'n': startscreen::hide(); stepPage(+1); break;
        case 'p': startscreen::hide(); stepPage(-1); break;
        case 'g': tankdlg::openManual(snap); break;
        case 'x': overlay::close(); break;
        case 'm': openMenu(); break;
        default: break;
      }
    }
    overlay::tick(now, touch::lastTouchMs());

    uint32_t wait = lv_timer_handler();
    if (wait > cfg::UI_LOOP_MAX_SLEEP_MS) wait = cfg::UI_LOOP_MAX_SLEEP_MS;
    vTaskDelay(pdMS_TO_TICKS(wait) > 0 ? pdMS_TO_TICKS(wait) : 1);
  }
}

}  // namespace ui
