// Touch-Controller FT6336U über I2C als LVGL-Eingabegerät, dazu die Wischgesten.
// Nur aus uiTask aufrufen.
#pragma once
#include <cstdint>
#include <lvgl.h>

#include "util/swipe.h"

namespace touch {

// Controller zurücksetzen und als LVGL-indev anmelden (Wire muss schon laufen)
lv_indev_t* init();

// Letzte erkannte Wischgeste abholen (danach wieder None)
Swipe takeSwipe();

// Wischgesten an/aus. Aus, solange ein Fenster offen ist (dort darf z. B. eine Liste scrollen).
void setSwipeEnabled(bool on);

// Touch um 180° drehen, passend zum Bild (Einbaulage)
void setFlipped(bool flipped);
// Touch-Test: ohne Fingerausgleich (reine Geometrie)
void setTestMode(bool on);

// Zeitpunkt der letzten Berührung (millis), für das Schließen von Fenstern nach 60 s
uint32_t lastTouchMs();

}  // namespace touch
