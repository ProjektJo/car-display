// Minimaler Hardwaretest (nur zur Fehlersuche, Umgebungen "hwtest" und "hwtest_ohne_psram"):
// kein LVGL, kein TFT_eSPI, keine Tasks. Das Hintergrundlicht blinkt im Sekundentakt, und der
// serielle Monitor schreibt "lebt ..." mit Flash- und PSRAM-Größe. Blinkt es, laufen Board,
// Flash-Modus und PSRAM-Einstellung; dann liegt ein Startproblem in der eigentlichen Firmware.
#include <Arduino.h>
#include <esp_system.h>

#ifdef HWTEST_PAD_KB
// Nur für den Größentest (hwtest_gross): großer Block im Flash, damit die Datei so groß wird wie die Firmware
__attribute__((used)) const uint8_t hwtestPad[HWTEST_PAD_KB * 1024] = {1};
#endif

namespace {
bool lightOn = true;

void setLight(bool on) {
  digitalWrite(BOARD_PIN_LCD_BL, (on == static_cast<bool>(BOARD_LCD_BL_ON_HIGH)) ? HIGH : LOW);
}
}  // namespace

void setup() {
  pinMode(BOARD_PIN_LCD_BL, OUTPUT);
  setLight(true);  // als Allererstes, noch vor dem Monitor

  Serial.begin(115200);
  const uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);
  Serial.printf("\nHardwaretest: Licht an Pin %d\n", BOARD_PIN_LCD_BL);
  Serial.printf("Neustart-Grund %d, Flash %u kB, PSRAM %u kB (frei %u kB)\n", static_cast<int>(esp_reset_reason()),
                static_cast<unsigned>(ESP.getFlashChipSize() / 1024), static_cast<unsigned>(ESP.getPsramSize() / 1024),
                static_cast<unsigned>(ESP.getFreePsram() / 1024));
#ifdef HWTEST_PAD_KB
  Serial.printf("Größentest: %u kB Polster, erstes Byte %u\n", static_cast<unsigned>(HWTEST_PAD_KB),
                static_cast<unsigned>(hwtestPad[0]));
#endif
}

void loop() {
  static uint32_t seconds = 0;
  Serial.printf("lebt %u s\n", static_cast<unsigned>(seconds++));
  lightOn = !lightOn;
  setLight(lightOn);
  delay(1000);
}
