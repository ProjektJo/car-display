// Display (ILI9341 über TFT_eSPI) als LVGL-Anzeige, dazu das Hintergrundlicht per PWM.
// Nur aus uiTask aufrufen.
#pragma once
#include <cstdint>
#include <lvgl.h>

namespace display {

// TFT, Zeichenpuffer und LVGL-Display anlegen. lv_init() muss vorher gelaufen sein.
lv_display_t* init();

// Helligkeit 0–100 %
void setBrightness(uint8_t percent);

}  // namespace display
