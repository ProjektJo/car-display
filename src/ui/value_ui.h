// Gemeinsame Fenster für Werte: Detail (Tippen, groß mit Verlauf) und Auswahl (lang drücken).
// Genutzt von den Kacheln der Übersicht und den frei belegbaren unteren Feldern (Eco, Sport, Sprint).
#pragma once
#include <lvgl.h>

#include "core/car_state.h"
#include "ui/values.h"

namespace valueui {

// Detail: Wert groß, bei Werten mit Verlauf die letzten 5 min mit Min/Max. Tippen schließt.
void openDetail(values::Key k);

// Auswahl: Liste aller Werte (scrollbar), der aktuelle ist markiert. cb bekommt den gewählten Wert.
using PickCb = void (*)(values::Key k, void* user);
void openPicker(const char* title, values::Key current, PickCb cb, void* user);

// Jede Bildschirm-Runde aufrufen (hält das Detail aktuell, auch über Seitenwechsel)
void tick(const CarSnapshot& s);

}  // namespace valueui
