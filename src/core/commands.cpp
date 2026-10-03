#include "commands.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "config.h"

namespace commands {

namespace {
QueueHandle_t obdQueue = nullptr;
QueueHandle_t calcQueue = nullptr;

bool send(QueueHandle_t q, const Command& c) { return q && xQueueSend(q, &c, 0) == pdTRUE; }
bool receive(QueueHandle_t q, Command& c, uint32_t waitMs) {
  return q && xQueueReceive(q, &c, pdMS_TO_TICKS(waitMs)) == pdTRUE;
}
}  // namespace

void init() {
  if (!obdQueue) obdQueue = xQueueCreate(cfg::CMD_QUEUE_LEN, sizeof(Command));
  if (!calcQueue) calcQueue = xQueueCreate(cfg::CMD_QUEUE_LEN, sizeof(Command));
}

bool toObd(const Command& c) { return send(obdQueue, c); }
bool toCalc(const Command& c) { return send(calcQueue, c); }
bool fromObd(Command& c, uint32_t waitMs) { return receive(obdQueue, c, waitMs); }
bool fromCalc(Command& c, uint32_t waitMs) { return receive(calcQueue, c, waitMs); }

}  // namespace commands
