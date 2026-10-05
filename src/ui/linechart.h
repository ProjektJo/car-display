// Liniendiagramm aus dem Verlauf (U Seite 7, Detailansicht): bis zu zwei Reihen, die erste in accent
// mit linker Achse, die zweite gestrichelt in muted mit rechter Achse. Achsen mit runden Werten
// (unten, Mitte, oben), Tempo, Drehzahl, Gaspedal und Verbrauch beginnen bei 0 (Vorschau lineChart).
// Gezeichnet im LVGL-Zeichenereignis eines Objekts.
#pragma once
#include <lvgl.h>

#include "ui/values.h"

namespace linechart {

struct Spec {
  values::Series series[2] = {values::Series::None, values::Series::None};
  int windowS = 300;     // 60, 300 oder 1800
  bool axes = true;
  int32_t x = 0, y = 0, w = 0, h = 0;  // Zeichenfläche relativ zum Objekt
};

// Zeichnet ins Objekt obj (Aufruf aus LV_EVENT_DRAW_MAIN)
void draw(lv_layer_t* layer, lv_obj_t* obj, const Spec& spec);

// Kleinster und größter Wert einer Reihe in den letzten windowS Sekunden; false ohne Wert
bool minMax(values::Series r, int windowS, float& lo, float& hi);

}  // namespace linechart
