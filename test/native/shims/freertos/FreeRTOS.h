// freertos/FreeRTOS.h — native shim. Just enough for CoreDelay.h and the
// manager headers that include FreeRTOS types.
#pragma once

#include <cstdint>

typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef void* TaskHandle_t;
typedef void* StackType_t;

#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#endif
#ifndef portMAX_DELAY
#define portMAX_DELAY ((TickType_t)0xFFFFFFFF)
#endif
#ifndef pdTRUE
#define pdTRUE 1
#endif
#ifndef pdFALSE
#define pdFALSE 0
#endif
#ifndef tskIDLE_PRIORITY
#define tskIDLE_PRIORITY 0
#endif
