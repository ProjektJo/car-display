#include "ui.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "config.h"
#include "core/car_state_store.h"
#include "core/commands.h"
#include "hw/display.h"
#include "hw/touch.h"
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

// Nächste bzw. vorige sichtbare Seite, Endlosschleife (U Bedienung)
void stepPage(int dir) {
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

}  // namespace

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
  carstate::snapshot(snap);
  createUi();
  showPage(current);
  Serial.println("Start: erstes Bild zeichnen");
  lv_refr_now(disp);
  display::setBrightness(cfg::BRIGHT_DAY_DEFAULT);  // Helligkeit aus dem Menü folgt in Etappe 7
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
      PAGES[current]->update(snap);
    }
    handleButton(now);
    handleSwipe();
    overlay::tick(now, touch::lastTouchMs());

    uint32_t wait = lv_timer_handler();
    if (wait > cfg::UI_LOOP_MAX_SLEEP_MS) wait = cfg::UI_LOOP_MAX_SLEEP_MS;
    vTaskDelay(pdMS_TO_TICKS(wait) > 0 ? pdMS_TO_TICKS(wait) : 1);
  }
}

}  // namespace ui
