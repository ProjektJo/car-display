// Car-Display: OBD2-Daten per BLE-OBD-Adapter lesen und auf dem Freenove ESP32-S3 Display anzeigen.
//
// Bedienung:
//   Tippen aufs Display (oder BOOT-Taste)  -> nächste Seite
//   Lang drücken (> 2 s)                    -> Fahrtdaten zurücksetzen

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "obd.h"
#include "ui.h"

namespace {

// FT6336U: Register 0x02 enthält die Anzahl der Berührungspunkte (0..2)
bool touched() {
  Wire.beginTransmission(TOUCH_I2C_ADDR);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(TOUCH_I2C_ADDR, (uint8_t)1) != 1) return false;
  uint8_t n = Wire.read() & 0x0F;
  return n >= 1 && n <= 2;
}

void beginTouch() {
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);
  delay(10);
  digitalWrite(TOUCH_RST, HIGH);
  delay(300);  // Controller braucht nach dem Reset etwas Zeit
  pinMode(TOUCH_INT, INPUT);
  Wire.begin(TOUCH_SDA, TOUCH_SCL, 400000);
}

constexpr uint8_t BOOT_BTN = 0;
constexpr uint32_t LONG_PRESS_MS = 2000;

uint32_t pressStart = 0;
bool longHandled = false;

void handleInput() {
  bool pressed = touched() || digitalRead(BOOT_BTN) == LOW;
  uint32_t now = millis();

  if (pressed) {
    if (!pressStart) {
      pressStart = now;
      longHandled = false;
    } else if (!longHandled && now - pressStart > LONG_PRESS_MS) {
      obd::resetTrip();
      longHandled = true;
      Serial.println("Fahrtdaten zurueckgesetzt");
    }
  } else if (pressStart) {
    // Loslassen nach kurzem Druck = Seite weiter (40 ms Entprellung)
    if (!longHandled && now - pressStart > 40) ui::nextPage();
    pressStart = 0;
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  pinMode(BOOT_BTN, INPUT_PULLUP);

  beginTouch();

  ui::begin();
  obd::begin();
}

void loop() {
  handleInput();
  obd::update();
  ui::update(obd::data());
}
