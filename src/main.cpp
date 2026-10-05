// Car-Display Firmware v2: Start der Tasks (A5)
//
//   Core 0: obdTask (bzw. Simulator), calcTask, später sensorTask
//   Core 1: uiTask (LVGL), storageTask (Flash)
#include <Arduino.h>
#include <Wire.h>

#include "config.h"
#include "core/calc_task.h"
#include "core/car_state_store.h"
#include "core/commands.h"
#include "storage/storage_task.h"
#include "ui/ui.h"
#ifdef SIMULATE_OBD
#include "sim/sim_task.h"
#else
#include "obd/obd_task.h"
#endif

void setup() {
  Serial.begin(115200);

  // Verstärker aus: keine Töne (M Hardware)
  pinMode(BOARD_PIN_AMP_EN, OUTPUT);
  digitalWrite(BOARD_PIN_AMP_EN, LOW);
  pinMode(BOARD_PIN_BOOT, INPUT_PULLUP);

  // Gemeinsamer I2C-Bus für Touch und später MPU6050
  Wire.begin(BOARD_PIN_I2C_SDA, BOARD_PIN_I2C_SCL, BOARD_I2C_FREQ_HZ);

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
  xTaskCreatePinnedToCore(calc::task, "calc", cfg::CALC_TASK_STACK, nullptr, cfg::CALC_TASK_PRIO, nullptr, cfg::CORE_DATA);
  xTaskCreatePinnedToCore(storage::task, "storage", cfg::STORAGE_TASK_STACK, nullptr, cfg::STORAGE_TASK_PRIO, nullptr,
                          cfg::CORE_UI);
}

void loop() {
  vTaskDelete(nullptr);  // alles läuft in eigenen Tasks
}
