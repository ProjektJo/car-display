#pragma once
#include <cstdint>
typedef uint32_t TickType_t;
typedef int BaseType_t;
#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY 0xFFFFFFFFu
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
void vTaskDelay(TickType_t t);
void vTaskDelayUntil(TickType_t* prev, TickType_t inc);
TickType_t xTaskGetTickCount();
void vTaskDelete(void*);
typedef void* SemaphoreHandle_t;
typedef void* QueueHandle_t;
typedef void (*TaskFunction_t)(void*);
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t, const char*, uint32_t, void*, unsigned, void*, int);
