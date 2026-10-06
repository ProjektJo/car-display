#include "touch.h"

#include <Arduino.h>
#include <Wire.h>

#include "hw/i2c_bus.h"

#include "config.h"

namespace touch {

namespace {
// Register des FT6336U
constexpr uint8_t REG_TD_STATUS = 0x02;  // Anzahl Berührungspunkte (untere 4 Bit), danach P1_XH, P1_XL, P1_YH, P1_YL
constexpr uint8_t READ_LEN = 5;
constexpr uint8_t MAX_POINTS = 2;
constexpr uint32_t RESET_LOW_MS = 10;
constexpr uint32_t RESET_BOOT_MS = 300;   // Controller braucht nach dem Reset etwas Zeit

SwipeDetector swipe(cfg::TAP_SLOP_PX, cfg::SWIPE_MIN_DX_PX, cfg::SWIPE_DOWN_MIN_DY_PX, cfg::SWIPE_DOWN_START_MAX_Y_PX);
Swipe pending = Swipe::None;
bool swipeEnabled = true;
bool flipped = false;
bool clickSuppressed = false;
uint32_t lastTouch = 0;
bool wasPressed = false;
int16_t lastX = 0, lastY = 0;

bool readRaw(uint16_t& rx, uint16_t& ry) {
  i2cbus::Guard bus;  // Bus mit dem MPU6050 geteilt
  Wire.beginTransmission(BOARD_TOUCH_I2C_ADDR);
  Wire.write(REG_TD_STATUS);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(static_cast<uint8_t>(BOARD_TOUCH_I2C_ADDR), READ_LEN) != READ_LEN) return false;
  uint8_t r[READ_LEN];
  for (uint8_t i = 0; i < READ_LEN; i++) r[i] = Wire.read();
  const uint8_t points = r[0] & 0x0F;
  if (points == 0 || points > MAX_POINTS) return false;
  rx = static_cast<uint16_t>(((r[1] & 0x0F) << 8) | r[2]);
  ry = static_cast<uint16_t>(((r[3] & 0x0F) << 8) | r[4]);
  return true;
}

// Rohkoordinaten (hochkant) in Display-Koordinaten (quer) umrechnen, Einstellungen im Board-Header
void mapPoint(uint16_t rx, uint16_t ry, int16_t& x, int16_t& y) {
  int32_t a = rx, b = ry;
  int32_t wA = BOARD_TOUCH_RAW_W, wB = BOARD_TOUCH_RAW_H;
  if (BOARD_TOUCH_SWAP_XY) {
    a = ry;
    b = rx;
    wA = BOARD_TOUCH_RAW_H;
    wB = BOARD_TOUCH_RAW_W;
  }
  if (BOARD_TOUCH_INVERT_X) a = wA - 1 - a;
  if (BOARD_TOUCH_INVERT_Y) b = wB - 1 - b;
  // Kalibrierung aus dem Board-Header (gemessener Versatz und Skalierung je Achse)
  a = static_cast<int32_t>(lroundf(a * BOARD_TOUCH_SCALE_X + BOARD_TOUCH_OFFSET_X));
  b = static_cast<int32_t>(lroundf(b * BOARD_TOUCH_SCALE_Y + BOARD_TOUCH_OFFSET_Y));
  if (flipped) {
    // Bild um 180° gedreht: x gespiegelt; y hat eine eigene Kalibrierung, weil der Sensor nicht
    // symmetrisch ist (gemessen in dieser Lage, schließt den Fingerausgleich schon ein)
    a = BOARD_LCD_HOR_RES - 1 - a;
    b = static_cast<int32_t>(lroundf(rx * BOARD_TOUCH_FLIP_SCALE_Y + BOARD_TOUCH_FLIP_OFFSET_Y));
  } else {
    b -= cfg::TOUCH_FINGER_OFFSET_Y_PX;  // Fingerausgleich (Bildschirmrichtung)
  }
  x = static_cast<int16_t>(constrain(a, 0, BOARD_LCD_HOR_RES - 1));
  y = static_cast<int16_t>(constrain(b, 0, BOARD_LCD_VER_RES - 1));
}

void readCb(lv_indev_t* indev, lv_indev_data_t* data) {
  uint16_t rx = 0, ry = 0;
  const bool pressed = readRaw(rx, ry);
  if (pressed) {
    mapPoint(rx, ry, lastX, lastY);
    lastTouch = millis();
    if (cfg::TOUCH_DEBUG_LOG && !wasPressed) Serial.printf("Touch: roh %u/%u -> x %d, y %d\n", rx, ry, lastX, lastY);
  }
  wasPressed = pressed;
  data->point.x = lastX;
  data->point.y = lastY;
  data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;

  if (!swipeEnabled) return;
  const Swipe s = swipe.update(pressed, lastX, lastY);
  if (s != Swipe::None) pending = s;
  // Sobald der Finger wandert, ist es kein Tippen und kein langes Drücken mehr:
  // LVGL schickt dem berührten Objekt dann "Druck verloren" statt "geklickt".
  if (swipe.movedWhileDown() && !clickSuppressed) {
    clickSuppressed = true;
    lv_indev_wait_release(indev);
  }
  if (!pressed) clickSuppressed = false;
}
}  // namespace

lv_indev_t* init() {
  pinMode(BOARD_PIN_TOUCH_INT, INPUT);
  pinMode(BOARD_PIN_TOUCH_RST, OUTPUT);
  digitalWrite(BOARD_PIN_TOUCH_RST, LOW);
  vTaskDelay(pdMS_TO_TICKS(RESET_LOW_MS));
  digitalWrite(BOARD_PIN_TOUCH_RST, HIGH);
  vTaskDelay(pdMS_TO_TICKS(RESET_BOOT_MS));

  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, readCb);
  lv_indev_set_long_press_time(indev, cfg::LONG_PRESS_MS);
  return indev;
}

void setFlipped(bool f) { flipped = f; }

Swipe takeSwipe() {
  const Swipe s = pending;
  pending = Swipe::None;
  return s;
}

void setSwipeEnabled(bool on) {
  if (on == swipeEnabled) return;
  swipeEnabled = on;
  swipe = SwipeDetector(cfg::TAP_SLOP_PX, cfg::SWIPE_MIN_DX_PX, cfg::SWIPE_DOWN_MIN_DY_PX, cfg::SWIPE_DOWN_START_MAX_Y_PX);
  pending = Swipe::None;
}

uint32_t lastTouchMs() { return lastTouch; }

}  // namespace touch
