#include "display.h"

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <esp_heap_caps.h>

#include "config.h"

namespace display {

namespace {
TFT_eSPI tft;
bool dmaActive = false;
bool pwmOk = false;
bool flippedNow = false;
uint8_t rotation() { return static_cast<uint8_t>((BOARD_LCD_ROTATION + (flippedNow ? 2 : 0)) % 4); }

void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* pxMap) {
  const uint32_t w = lv_area_get_width(area);
  const uint32_t h = lv_area_get_height(area);
  uint16_t* px = reinterpret_cast<uint16_t*>(pxMap);
  if (dmaActive) {
    // Vorige Übertragung abwarten, dann diese starten. LVGL darf sofort in den zweiten
    // Puffer zeichnen; bevor es diesen Puffer wieder benutzt, wartet der nächste Flush hier.
    tft.dmaWait();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushPixelsDMA(px, w * h);
  } else {
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushPixels(px, w * h);
    tft.endWrite();
  }
  lv_display_flush_ready(disp);
}
}  // namespace

lv_display_t* init() {
  // Hintergrundlicht zuerst aus, damit beim Start kein Bildmüll zu sehen ist; an geht es nach der Startzeile
  pwmOk = ledcSetup(cfg::BACKLIGHT_PWM_CHANNEL, cfg::BACKLIGHT_PWM_FREQ_HZ, cfg::BACKLIGHT_PWM_BITS) > 0;
  if (pwmOk) {
    ledcAttachPin(BOARD_PIN_LCD_BL, cfg::BACKLIGHT_PWM_CHANNEL);
  } else {
    Serial.println("Display: PWM fürs Licht nicht verfügbar, Licht nur an/aus");
    pinMode(BOARD_PIN_LCD_BL, OUTPUT);
  }
  setBrightness(0);

  Serial.println("Start: Display-Controller wird eingerichtet");
  tft.init();
  tft.setRotation(rotation());
  tft.setSwapBytes(true);  // LVGL rechnet RGB565 little-endian, das Display erwartet big-endian
  tft.fillScreen(TFT_BLACK);

  // Startzeile direkt mit TFT_eSPI, dann Licht an: Bleibt danach etwas hängen, ist wenigstens
  // diese Zeile zu sehen. So lässt sich ein Display-Fehler von einem späteren Fehler trennen.
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.drawString("Car-Display startet", BOARD_LCD_HOR_RES / 2, BOARD_LCD_VER_RES / 2);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  setBrightness(cfg::BRIGHT_DAY_DEFAULT);
  Serial.println("Start: Display und Licht an");
#if DISPLAY_USE_DMA
  dmaActive = tft.initDMA();
  if (dmaActive) tft.startWrite();  // Bus bleibt für DMA dauerhaft belegt (nur das Display hängt daran)
  Serial.printf("Display: DMA %s\n", dmaActive ? "an" : "nicht verfügbar, ohne DMA");
#endif

  // Zwei Zeichenpuffer im internen, DMA-fähigen RAM (M: 2 x 320 x 40 Pixel)
  const size_t bufBytes = BOARD_LCD_HOR_RES * cfg::DRAW_BUF_LINES * sizeof(uint16_t);
  void* buf1 = heap_caps_malloc(bufBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  void* buf2 = heap_caps_malloc(bufBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!buf1 || !buf2) {
    Serial.println("Display: kein Speicher für die Zeichenpuffer");
    return nullptr;
  }

  lv_display_t* disp = lv_display_create(BOARD_LCD_HOR_RES, BOARD_LCD_VER_RES);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(disp, flushCb);
  lv_display_set_buffers(disp, buf1, buf2, bufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  return disp;
}

void setFlipped(bool flipped) {
  if (flipped == flippedNow) return;
  flippedNow = flipped;
  if (!lv_display_get_default()) return;  // vor init(): gilt ab dem Start
  if (dmaActive) tft.dmaWait();
  tft.setRotation(rotation());
  lv_obj_invalidate(lv_screen_active());
  lv_obj_invalidate(lv_layer_top());
}

void setBrightness(uint8_t percent) {
  if (percent > 100) percent = 100;
  if (!pwmOk) {
    const bool on = percent > 0;
    digitalWrite(BOARD_PIN_LCD_BL, (on == static_cast<bool>(BOARD_LCD_BL_ON_HIGH)) ? HIGH : LOW);
    return;
  }
  const uint32_t maxDuty = (1u << cfg::BACKLIGHT_PWM_BITS) - 1;
  uint32_t duty = maxDuty * percent / 100;
  if (!BOARD_LCD_BL_ON_HIGH) duty = maxDuty - duty;
  ledcWrite(cfg::BACKLIGHT_PWM_CHANNEL, duty);
}

}  // namespace display
