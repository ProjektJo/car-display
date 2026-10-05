// Fenster über der aktuellen Seite (Menü, Dialoge, Tank-Fenster) auf lv_layer_top().
// Abgedunkelter Hintergrund und Karte wie in der Vorschau (.ov, .dlg).
// Alle Fenster außer dem Tank-Fenster schließen nach 60 s ohne Berührung (M, U Bedienung).
#pragma once
#include <cstdint>
#include <lvgl.h>

#include "ui/theme.h"

namespace overlay {

// Öffnet ein Fenster und liefert die Karte, in die der Inhalt kommt. Ein offenes Fenster wird ersetzt.
// autoClose = false nur für das Tank-Fenster. inset = Abstand der Karte zum Displayrand
// (Vorschau: Menü 14 px, Unterdialoge 6 px).
lv_obj_t* open(const char* title, bool autoClose = true, int32_t inset = theme::DIALOG_INSET);
void close();
bool isOpen();

// Zählt jedes geöffnete Fenster hoch. Wer Inhalte eines Fensters später ändert, merkt sich die
// Nummer beim Öffnen und prüft sie, denn ein anderes Fenster kann es inzwischen ersetzt haben.
uint32_t generation();

// Knopf "Fertig" oben rechts in der Karte (Vorschau: 54 x 22 px, 8 px vom Rand)
lv_obj_t* addDoneButton(lv_obj_t* card, lv_event_cb_t onClick);

// Einmal je UI-Durchlauf: schließt nach Ablauf der Zeit ohne Berührung
void tick(uint32_t nowMs, uint32_t lastTouchMs);

}  // namespace overlay
