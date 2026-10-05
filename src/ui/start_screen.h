// Startbildschirm: Verbindungsstatus in Schritten statt eines Logos (A2 Nr. 23, U Startbildschirm).
// "Suche Adapter …" -> "Verbunden mit vLinker MC" -> "Protokoll: ISO 14230" -> "Fahrzeug: Renault Modus",
// bei Fehlern ein Satz mit der Lösung.
//
// ANNAHME: Der Startbildschirm erscheint nur bis zur ersten Verbindung nach dem Einschalten. Reißt
// die Verbindung später ab, zeigt das nur der Verbindungspunkt, damit die Seite beim Fahren bleibt.
// ANNAHME: Tippen oder Wischen blendet ihn aus (Seiten ohne Auto ansehen); lang drücken öffnet das Menü.
#pragma once
#include <lvgl.h>

#include "core/car_state.h"

namespace startscreen {

// Legt den Bildschirm über den Seiten und der Statusleiste an. onLongPress öffnet das Menü.
void create(lv_obj_t* parent, lv_event_cb_t onLongPress);

// Einmal je Snapshot
void update(const CarSnapshot& s);

bool visible();
void hide();

}  // namespace startscreen
