// CoreDelay.h
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/**
 * @brief Yield in FreeRTOS-friendly chunks until approximately timeInMs has elapsed.
 *
 * Splits the total delay into multiple vTaskDelay() calls of up to CHUNK_MS each,
 * so that other tasks (including IDLE) get scheduled frequently.
 *
 * @param timeInMs Total delay in milliseconds.
 */
static inline void coreDelay(unsigned long timeInMs)
{
    const TickType_t CHUNK_MS = 100; // max chunk size
    const TickType_t chunkTicks = pdMS_TO_TICKS(CHUNK_MS);
    TickType_t remaining = pdMS_TO_TICKS(timeInMs);

    while (remaining > 0)
    {
        TickType_t delayTicks = (remaining > chunkTicks)
                                    ? chunkTicks
                                    : remaining;
        vTaskDelay(delayTicks);
        remaining -= delayTicks;
    }
}