#include "i2c_bus.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace i2cbus {

namespace {
SemaphoreHandle_t mutex = nullptr;
}

void init() {
  if (!mutex) mutex = xSemaphoreCreateMutex();
}

void lock() {
  if (mutex) xSemaphoreTake(mutex, portMAX_DELAY);
}

void unlock() {
  if (mutex) xSemaphoreGive(mutex);
}

}  // namespace i2cbus
