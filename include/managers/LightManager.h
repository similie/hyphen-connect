// File: LightManager.h

#ifndef LIGHT_MANAGER_H
#define LIGHT_MANAGER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "CoreDelay.h"
class LightManager
{
private:
    // PWM configuration
    static constexpr uint8_t pwmChannel = 0;
    static constexpr uint32_t pwmFrequency = 5000;
    static constexpr uint8_t pwmResolution = 8;

    // FreeRTOS task handles
    TaskHandle_t breathHandle = nullptr;
    TaskHandle_t flashHandle = nullptr;
    volatile bool stopBreathing = false;
    void processFlash(long);
    // Flash control
    long flashDuration = -1;
    bool flashOn = false;

    // Internal LED routines
    void pinLoop();
    void flash();

    // Task entry points
    static void runBreathing(void *param);
    static void runFlash(void *param);

public:
    LightManager();

    // Breathing effect
    void startBreathing();
    void endBreathing();

    // Instant states
    void bright();
    void dim();
    void off();

    // Flash effect
    void startFlash();
    void endFlash();
    void flashFor(long durationMs);
};

#endif // LIGHT_MANAGER_H