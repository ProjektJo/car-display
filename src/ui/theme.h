// Farben, Schriften und gemeinsame Stile (M Gestaltung, U Gestaltung).
// Farbe trägt nur Bedeutung (sparsam / normal / Achtung), sonst Grautöne.
#pragma once
#include <cstdint>
#include <lvgl.h>

// Schriften (src/ui/fonts, erzeugt mit tools/make_fonts.sh): Montserrat mit Umlauten
LV_FONT_DECLARE(font_m12)  // Beschriftung, Statusleiste
LV_FONT_DECLARE(font_m14)  // Text
LV_FONT_DECLARE(font_m20)  // Kachelwert
LV_FONT_DECLARE(font_m28)  // Seitenwert
LV_FONT_DECLARE(font_m48)  // groß
LV_FONT_DECLARE(font_d72)  // Großanzeige, nur Ziffern , . - / : und –
// Kleine Schrift 10 px (LVGL-Montserrat, nur ASCII); fehlende Zeichen (Umlaute, Symbole) aus font_m12
extern lv_font_t font_small;

namespace theme {

// Farbtokens (M, Tabelle Farbtokens)
constexpr uint32_t BG = 0x07090B;  // 6.10.2026 dunkler für mehr Kontrast (Jos Wunsch, vorher 0x0E1114)
constexpr uint32_t SURFACE = 0x171B20;
constexpr uint32_t LINE = 0x262C33;
constexpr uint32_t TEXT = 0xE8EAED;
constexpr uint32_t TEXT_NIGHT = 0xC9CDD2;  // Nachtmodus: Text abgedunkelt (U Gestaltung)
constexpr uint32_t MUTED = 0x8B949E;
constexpr uint32_t ACCENT = 0x7FA7C4;
constexpr uint32_t GOOD = 0x5FB98B;
constexpr uint32_t WARN = 0xD6A24A;
constexpr uint32_t BAD = 0xD46A5E;
constexpr uint32_t OVERLAY_DIM = 0x030405;  // Hintergrund hinter Fenstern (Vorschau: rgba(8,10,12,.82))
constexpr lv_opa_t OVERLAY_DIM_OPA = 209;   // 0,82 · 255

inline lv_color_t c(uint32_t hex) { return lv_color_hex(hex); }

// Maße aus der Vorschau (ein CSS-px = ein Display-Pixel)
constexpr int32_t STATUSBAR_H = 20;
constexpr int32_t CONTENT_H = 220;
constexpr int32_t RADIUS_TILE = 6;
constexpr int32_t RADIUS_DIALOG = 8;
constexpr int32_t DIALOG_INSET = 14;      // Menü (.dlg)
constexpr int32_t DIALOG_INSET_SUB = 6;   // Unterdialoge wie Diagnose

// Theme anwenden (Standard-Theme dunkel, Akzent), Hintergrund setzen
void init(lv_display_t* disp);

// Nachtmodus: dunklerer Text. Wirkt auf alle Objekte mit textStyle().
void setNight(bool night);
bool night();

// Gemeinsame Stile
lv_style_t* textStyle();    // Werte und Text in `text` (bzw. Nachtfarbe)
lv_style_t* mutedStyle();   // Beschriftungen und Einheiten in `muted`

// Kleine Helfer für Beschriftungen
lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, bool muted, const char* text = "");

}  // namespace theme
