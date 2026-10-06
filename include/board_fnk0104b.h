/*
 * Board-Beschreibung: Freenove ESP32-S3 Display 2,8" mit Touch, FNK0104B
 * (Modul ESP32-S3 N16R8: 16 MB Flash, 8 MB OPI-PSRAM).
 *
 * Alle Pins und Board-Eigenheiten stehen nur hier. Ein anderes Board braucht nur eine
 * andere Datei dieser Art (Name in platformio.ini bei "-include" tauschen).
 *
 * Die Datei wird per "-include" in jede Quelldatei eingebunden, auch in TFT_eSPI und in
 * die C-Dateien von LVGL. Deshalb stehen hier nur #define, kein C++.
 * Quelle der Pins: Freenove-Repo "Freenove_ESP32_S3_Display", Konfiguration
 * "FNK0104AB_2.8_240x320_ILI9341" (siehe Architektur, Kapitel 3).
 */
#pragma once

#define BOARD_NAME "Freenove FNK0104B"

/* --------------------------------------------------------------------------- */
/* Display ILI9341, SPI, im Querformat betrieben */
/* --------------------------------------------------------------------------- */
#define BOARD_LCD_HOR_RES   320   /* Breite im Querformat */
#define BOARD_LCD_VER_RES   240   /* Höhe im Querformat */
#define BOARD_LCD_ROTATION  1     /* TFT_eSPI-Rotation: quer, USB-Buchse rechts (bewährt aus der alten Firmware) */
#define BOARD_PIN_LCD_BL    45    /* Hintergrundlicht, PWM */
#define BOARD_LCD_BL_ON_HIGH 1    /* 1 = HIGH schaltet das Licht ein */

/* TFT_eSPI-Konfiguration, übernommen aus den getesteten Build-Flags der alten Firmware. */
/* Das Hintergrundlicht (TFT_BL) ist hier absichtlich nicht gesetzt: Es läuft über PWM */
/* (hw/display.cpp), TFT_eSPI soll den Pin nicht anfassen. */
#define USER_SETUP_LOADED   1
#define ILI9341_2_DRIVER    1
#define TFT_RGB_ORDER       TFT_BGR
#define TFT_INVERSION_ON    1
#define TFT_WIDTH           240   /* native Ausrichtung (hochkant) */
#define TFT_HEIGHT          320
#define TFT_MISO            13
#define TFT_MOSI            11
#define TFT_SCLK            12
#define TFT_CS              10
#define TFT_DC              46
#define TFT_RST             -1    /* kein Reset-Pin */
/* SPI-Port: Freenove nutzt USE_HSPI_PORT, aber mit Arduino-Kern 3.x. Mit unserem Kern 2.0.x zeigt  */
/* TFT_eSPI damit vermutlich auf das Register des Flash-Controllers (SPI1) statt auf das Display:     */
/* Board stürzt beim tft.init ab. FSPI ist der Display-Bus SPI2, die Pins laufen über die GPIO-Matrix. */
#define USE_FSPI_PORT       1
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  16000000
#define DISABLE_ALL_LIBRARY_WARNINGS 1  /* keine TFT_eSPI-Hinweise wie "TOUCH_CS nicht definiert" */
#define LOAD_GLCD           1     /* kleine Standardschrift nur für die Startzeile vor LVGL (hw/display.cpp) */

/* --------------------------------------------------------------------------- */
/* I2C-Bus (geteilt: Touch, Audio-Codec ES8311, optional MPU6050) */
/* --------------------------------------------------------------------------- */
#define BOARD_PIN_I2C_SDA   16
#define BOARD_PIN_I2C_SCL   15
#define BOARD_I2C_FREQ_HZ   400000

/* Touch FT6336U */
#define BOARD_TOUCH_I2C_ADDR 0x38
#define BOARD_PIN_TOUCH_RST 18
#define BOARD_PIN_TOUCH_INT 17
#define BOARD_TOUCH_RAW_W   240   /* Rohkoordinaten des Controllers (hochkant) */
#define BOARD_TOUCH_RAW_H   320
/* Umrechnung Rohkoordinaten -> Querformat. Abhängig von der Einbaulage des Touch-Panels. */
/* ANNAHME: x = roh_y, y = 239 - roh_x. Stimmt die Richtung nicht, hier umstellen */
/* (der serielle Monitor zeigt beim Tippen Roh- und umgerechnete Werte). */
#define BOARD_TOUCH_SWAP_XY  1
#define BOARD_TOUCH_INVERT_X 0
#define BOARD_TOUCH_INVERT_Y 1
/* Kalibrierung nach dem Umrechnen: Display = Wert · SCALE + OFFSET. Gemessen am 6.10.2026 mit dem   */
/* Touch-Test (Fadenkreuze, serielles "t"): y lag oben bei 38 statt 20 und unten bei 211 statt 219.   */
/* x stimmte.                                                                                         */
#define BOARD_TOUCH_SCALE_X  1.0f
#define BOARD_TOUCH_OFFSET_X 0.0f
#define BOARD_TOUCH_SCALE_Y  1.150f
#define BOARD_TOUCH_OFFSET_Y (-23.7f)

#define BOARD_IMU_I2C_ADDR   0x68 /* optional MPU6050 (Etappe 8) */
#define BOARD_CODEC_I2C_ADDR 0x18 /* ES8311, nicht genutzt */

/* --------------------------------------------------------------------------- */
/* Tasten, Audio, LED */
/* --------------------------------------------------------------------------- */
#define BOARD_PIN_BOOT      0     /* BOOT-Taste, gedrückt = LOW */
#define BOARD_PIN_AMP_EN    1     /* Verstärker-Enable, bleibt dauerhaft LOW (keine Töne) */
#define BOARD_PIN_RGB_LED   42    /* WS2812, nicht genutzt */
#define BOARD_PIN_BAT_ADC   9     /* Akku-Messung, nicht genutzt */

/* --------------------------------------------------------------------------- */
/* Optionale Erweiterungen */
/* --------------------------------------------------------------------------- */
/* GPS NEO-6M / NEO-M8N an UART1 (Etappe 8) */
#define BOARD_PIN_GPS_RX    2     /* ESP empfängt hier (an TX des GPS) */
#define BOARD_PIN_GPS_TX    3
#define BOARD_GPS_BAUD      9600

/* microSD (SD_MMC, 4 Bit), nur für den Export (Etappe 8) */
#define BOARD_PIN_SD_CMD    40
#define BOARD_PIN_SD_CLK    38
#define BOARD_PIN_SD_D0     39
#define BOARD_PIN_SD_D1     41
#define BOARD_PIN_SD_D2     48
#define BOARD_PIN_SD_D3     47
