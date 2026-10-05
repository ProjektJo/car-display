// Gemeinsame Schnittstelle aller Seiten (M, Code-Struktur): create(parent), update(snapshot), onShow().
#pragma once
#include <lvgl.h>

#include "core/car_state.h"

class Page {
 public:
  explicit Page(const char* name) : name_(name) {}
  virtual ~Page() = default;

  // Name in der Statusleiste
  const char* name() const { return name_; }

  // Einmal beim Start: Inhalt in parent (320 x 220 px unter der Statusleiste) anlegen
  virtual void create(lv_obj_t* parent) = 0;
  // 10-mal pro Sekunde, nur solange die Seite sichtbar ist. Nur geänderte Werte neu setzen.
  virtual void update(const CarSnapshot& s) = 0;
  // Beim Einblenden der Seite
  virtual void onShow() {}
  // 10-mal pro Sekunde für jede Seite, auch unsichtbar (z. B. Spartipps und Sperrzeiten weiterzählen)
  virtual void tick(const CarSnapshot&) {}
  // false = Seite ausgeblendet (z. B. G-Kraft ohne MPU6050)
  virtual bool available(const CarSnapshot&) const { return true; }

 private:
  const char* name_;
};

// Platzhalter für Seiten, die in einer späteren Etappe gebaut werden
class PlaceholderPage : public Page {
 public:
  PlaceholderPage(const char* name, int stage) : Page(name), stage_(stage) {}
  void create(lv_obj_t* parent) override;
  void update(const CarSnapshot&) override {}

 private:
  int stage_;
};

// Alle Seiten in Wischreihenfolge (A2 Nr. 8, U Seiten)
Page* ecoPage();
Page* sportPage();
Page* sprintPage();
Page* overviewPage();
Page* bigPage();
Page* tripTankPage();
Page* chartsPage();
Page* gforcePage();
Page* historyPage();
Page* dtcPage();
Page* infoPage();
