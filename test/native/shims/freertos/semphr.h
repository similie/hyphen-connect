// freertos/semphr.h — native shim. Recursive-mutex calls become no-ops (tests
// are single-threaded).
#pragma once

#include <freertos/FreeRTOS.h>

typedef void* SemaphoreHandle_t;

inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() { return (void*)1; }
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return (void*)1; }
inline BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t, TickType_t) {
  return pdTRUE;
}
inline BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t) { return pdTRUE; }
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t) {
  return pdTRUE;
}
inline BaseType_t xSemaphoreGive(SemaphoreHandle_t) { return pdTRUE; }
