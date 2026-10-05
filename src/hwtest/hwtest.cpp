// Minimaler Hardwaretest (nur zur Fehlersuche, Umgebungen "hwtest..."): keine Firmware, kein LVGL, keine Tasks.
//
// Ohne HWTEST_DISPLAY: Das Hintergrundlicht blinkt im Sekundentakt, der serielle Monitor schreibt
// "lebt ..." mit Flash- und PSRAM-Größe.
// Mit HWTEST_DISPLAY (hwtest_display): Licht bleibt an, das Display zeigt im Sekundentakt Rot, Grün,
// Blau und Weiß mit großer Schrift. Damit ist geprüft, ob Display, Kabel und TFT-Einstellung zusammenpassen.
#include <Arduino.h>
#include <esp_system.h>
#ifdef HWTEST_DISPLAY
#include <TFT_eSPI.h>
#endif

#ifdef HWTEST_PAD_KB
// Nur für den Größentest (hwtest_gross): großer Block im Flash, damit die Datei so groß wird wie die Firmware
__attribute__((used)) const uint8_t hwtestPad[HWTEST_PAD_KB * 1024] = {1};
#endif

namespace {
bool lightOn = true;

void setLight(bool on) {
  digitalWrite(BOARD_PIN_LCD_BL, (on == static_cast<bool>(BOARD_LCD_BL_ON_HIGH)) ? HIGH : LOW);
}

#ifdef HWTEST_DISPLAY
TFT_eSPI tft;

struct TestColor {
  uint16_t color;
  uint16_t text;
  const char* name;
};
constexpr TestColor COLORS[] = {
    {TFT_RED, TFT_WHITE, "ROT"},
    {TFT_GREEN, TFT_BLACK, "GRUEN"},
    {TFT_BLUE, TFT_WHITE, "BLAU"},
    {TFT_WHITE, TFT_BLACK, "WEISS"},
};
#endif
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
#ifdef HWTEST_DISPLAY
  Serial.println("Displaytest: tft.init");
  tft.init();
  tft.setRotation(BOARD_LCD_ROTATION);
  tft.setTextDatum(MC_DATUM);
  Serial.println("Displaytest: läuft");
#endif
}

void loop() {
  static uint32_t seconds = 0;
  Serial.printf("lebt %u s\n", static_cast<unsigned>(seconds));
#ifdef HWTEST_DISPLAY
  const TestColor& c = COLORS[seconds % (sizeof(COLORS) / sizeof(COLORS[0]))];
  tft.fillScreen(c.color);
  tft.setTextColor(c.text, c.color);
  tft.setTextSize(4);
  tft.drawString(c.name, BOARD_LCD_HOR_RES / 2, BOARD_LCD_VER_RES / 2);
#else
  lightOn = !lightOn;
  setLight(lightOn);
#endif
  seconds++;
  delay(1000);
}
