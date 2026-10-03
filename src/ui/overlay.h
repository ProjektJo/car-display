// Fenster über der aktuellen Seite (Menü, Dialoge, Tank-Fenster) auf lv_layer_top().
// Abgedunkelter Hintergrund und Karte wie in der Vorschau (.ov, .dlg).
// Alle Fenster außer dem Tank-Fenster schließen nach 60 s ohne Berührung (M, U Bedienung).
#pragma once
#include <cstdint>
#include <lvgl.h>

namespace overlay {

// Öffnet ein Fenster und liefert die Karte, in die der Inhalt kommt. Ein offenes Fenster wird ersetzt.
// autoClose = false nur für das Tank-Fenster.
lv_obj_t* open(const char* title, bool autoClose = true);
void close();
bool isOpen();

// Einmal je UI-Durchlauf: schließt nach Ablauf der Zeit ohne Berührung
void tick(uint32_t nowMs, uint32_t lastTouchMs);

}  // namespace overlay
