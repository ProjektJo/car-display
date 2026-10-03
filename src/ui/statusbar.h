// Statusleiste, 20 px (U Rahmen jeder Seite): Verbindungspunkt, Seitenname, Seitenpunkte,
// Motor-Symbol bei MIL, Tanksymbol mit Litern, Uhrzeit nur mit GPS.
#pragma once
#include <lvgl.h>

#include "core/car_state.h"

namespace statusbar {

constexpr int MAX_PAGES = 11;

// Legt die Leiste an. onLongPress wird bei langem Druck auf die Leiste gerufen (Menü).
void create(lv_obj_t* parent, lv_event_cb_t onLongPress);

// Seitenname und Punkte (index von count sichtbaren Seiten)
void setPage(const char* name, int index, int count);

// Werte aus dem Snapshot; ändert nur, was sich geändert hat
void update(const CarSnapshot& s);

}  // namespace statusbar
