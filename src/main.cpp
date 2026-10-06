// Car-Display Firmware v2: Start der Tasks (A5)
//
//   Core 0: obdTask (bzw. Simulator), calcTask, später sensorTask
//   Core 1: uiTask (LVGL), storageTask (Flash)
#include <Arduino.h>
#include <Wire.h>
#include <esp_system.h>

#include "config.h"
#include "core/calc_task.h"
#include "core/car_state_store.h"
#include "core/commands.h"
#include "hw/i2c_bus.h"
#include "storage/storage_task.h"
#include "ui/ui.h"
#ifdef SIMULATE_OBD
#include "sim/sim_task.h"
#else
#include "obd/obd_task.h"
#include "sensors/sensor_task.h"
#endif

namespace {
// Grund des letzten Neustarts lesbar ausgeben: zeigt Absturzschleifen (Panic, Watchdog, Brownout)
const char* resetText(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "Einschalten";
    case ESP_RST_SW: return "Software-Neustart";
    case ESP_RST_PANIC: return "ABSTURZ (Panic)";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "WATCHDOG";
    case ESP_RST_BROWNOUT: return "SPANNUNG ZU NIEDRIG (Brownout)";
    case ESP_RST_DEEPSLEEP: return "Tiefschlaf";
    case ESP_RST_EXT: return "Reset-Taste";
    default: return "unbekannt";
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  // USB-Seriell kurz abwarten, damit die ersten Zeilen im Monitor ankommen. Ohne PC läuft es
  // nach der Frist einfach weiter.
  const uint32_t serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < cfg::SERIAL_WAIT_MS) delay(10);
  Serial.printf("\nStart: Neustart-Grund %s, PSRAM %u kB\n", resetText(esp_reset_reason()),
                (unsigned)(ESP.getPsramSize() / 1024));

  // Verstärker aus: keine Töne (M Hardware)
  pinMode(BOARD_PIN_AMP_EN, OUTPUT);
  digitalWrite(BOARD_PIN_AMP_EN, LOW);
  pinMode(BOARD_PIN_BOOT, INPUT_PULLUP);

  // Gemeinsamer I2C-Bus für Touch und später MPU6050
  Wire.begin(BOARD_PIN_I2C_SDA, BOARD_PIN_I2C_SCL, BOARD_I2C_FREQ_HZ);
  i2cbus::init();

  carstate::init();
  commands::init();
  calc::init();
  storage::init();
  Serial.printf("\nCar-Display %s (%s)%s\n", cfg::FW_VERSION, BOARD_NAME,
#ifdef SIMULATE_OBD
                " - SIMULATOR"
#else
                ""
#endif
  );

  xTaskCreatePinnedToCore(ui::task, "ui", cfg::UI_TASK_STACK, nullptr, cfg::UI_TASK_PRIO, nullptr, cfg::CORE_UI);
#ifdef SIMULATE_OBD
  xTaskCreatePinnedToCore(sim::task, "obd", cfg::OBD_TASK_STACK, nullptr, cfg::OBD_TASK_PRIO, nullptr, cfg::CORE_DATA);
#else
  xTaskCreatePinnedToCore(obd::task, "obd", cfg::OBD_TASK_STACK, nullptr, cfg::OBD_TASK_PRIO, nullptr, cfg::CORE_DATA);
#endif
#ifndef SIMULATE_OBD
  // Optionale Sensoren (MPU6050, GPS); im Simulator spielt sim::task auch diese nach
  xTaskCreatePinnedToCore(sensors::task, "sensor", cfg::SENSOR_TASK_STACK, nullptr, cfg::SENSOR_TASK_PRIO, nullptr,
                          cfg::CORE_DATA);
#endif
  xTaskCreatePinnedToCore(calc::task, "calc", cfg::CALC_TASK_STACK, nullptr, cfg::CALC_TASK_PRIO, nullptr, cfg::CORE_DATA);
  xTaskCreatePinnedToCore(storage::task, "storage", cfg::STORAGE_TASK_STACK, nullptr, cfg::STORAGE_TASK_PRIO, nullptr,
                          cfg::CORE_UI);
}

void loop() {
  vTaskDelete(nullptr);  // alles läuft in eigenen Tasks
}
