// freertos/task.h — native shim. vTaskDelay is a no-op so coreDelay() returns
// instantly in tests.
#pragma once

#include <freertos/FreeRTOS.h>

inline void vTaskDelay(TickType_t) {}
inline TaskHandle_t xTaskGetCurrentTaskHandle() { return nullptr; }
inline void vTaskDelete(TaskHandle_t) {}
