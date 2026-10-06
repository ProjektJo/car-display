// uiTask (Core 1): LVGL, Touch, Gesten, BOOT-Taste, Seitenwechsel (A5).
// Nur diese Task ruft LVGL auf.
#pragma once

namespace ui {

void task(void* arg);

// Für Menü und Dialoge (nur aus uiTask)
void applyBrightness();  // Helligkeit und Nachtmodus aus den Einstellungen
void sendGoal();         // Spar-Ziel aus den Einstellungen an calcTask
void showInfoPage();     // Menü → Info

}  // namespace ui
