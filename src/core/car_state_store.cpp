#include "car_state_store.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace carstate {

namespace {
CarState state;
uint32_t seq = 0;
SemaphoreHandle_t mutex = nullptr;
}  // namespace

void init() {
  if (!mutex) mutex = xSemaphoreCreateMutex();
}

CarState& lock() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  return state;
}

void unlock() {
  seq++;
  xSemaphoreGive(mutex);
}

void snapshot(CarSnapshot& out) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  static_cast<CarState&>(out) = state;
  out.seq = seq;
  xSemaphoreGive(mutex);
  out.now = millis();
}

}  // namespace carstate
