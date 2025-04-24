// File: LightManager.cpp

#include "managers/LightManager.h"

// Constructor: configure the PWM channel
LightManager::LightManager()
{
    ledcSetup(pwmChannel, pwmFrequency, pwmResolution);
    ledcAttachPin(LED_PIN, pwmChannel);
}

// Fade in/out over 8 ms per step
void LightManager::pinLoop()
{
    // Fade in
    for (int duty = 0; duty <= 255; ++duty)
    {
        ledcWrite(pwmChannel, duty);
        vTaskDelay(pdMS_TO_TICKS(8));
    }
    // Fade out
    for (int duty = 255; duty >= 0; --duty)
    {
        ledcWrite(pwmChannel, duty);
        vTaskDelay(pdMS_TO_TICKS(8));
    }
}

// Toggle between off & bright
void LightManager::flash()
{
    if (flashOn)
    {
        off();
    }
    else
    {
        bright();
    }
    flashOn = !flashOn;
}

// Task to run a timed flash loop
void LightManager::runFlash(void *param)
{
    auto *light = static_cast<LightManager *>(param);
    unsigned long start = millis();

    while (true)
    {
        light->flash();
        if (light->flashDuration >= 0 &&
            millis() - start > (unsigned long)light->flashDuration)
        {
            light->endFlash();
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Task to run continuous breathing (flash + endless fade)
void LightManager::runBreathing(void *param)
{
    auto *light = static_cast<LightManager *>(param);
    // first a 3 s flash to indicate start
    light->flashDuration = 3000;
    runFlash(param);
    light->flashDuration = -1;
    // then endless fade in/out
    while (true)
    {
        light->pinLoop();
    }
}

// Start the breathing effect on core 1 at prio tskIDLE+1
void LightManager::startBreathing()
{
    if (breathHandle)
        return; // already running

    xTaskCreatePinnedToCore(
        runBreathing,
        "RunBreathingTask",
        4096,
        this,
        tskIDLE_PRIORITY + 1, // below your network runner
        &breathHandle,
        1 // core 1
    );
}

// Stop breathing and leave a 3 s flash
void LightManager::endBreathing()
{
    if (!breathHandle)
        return;

    vTaskDelete(breathHandle);
    breathHandle = nullptr;
    flashFor(3000);
}

// Turn LED fully on
void LightManager::bright()
{
    ledcWrite(pwmChannel, 255);
}

// Half brightness
void LightManager::dim()
{
    ledcWrite(pwmChannel, 128);
}

// Turn LED off
void LightManager::off()
{
    ledcWrite(pwmChannel, 0);
}

// Start a flashing task on core 1 at prio tskIDLE+1
void LightManager::startFlash()
{
    if (flashHandle)
        return; // already running

    xTaskCreatePinnedToCore(
        runFlash,
        "RunFlashingTask",
        4096,
        this,
        tskIDLE_PRIORITY + 1,
        &flashHandle,
        1 // core 1
    );
}

// Stop flashing and restore steady bright
void LightManager::endFlash()
{
    if (!flashHandle)
        return;

    vTaskDelete(flashHandle);
    flashHandle = nullptr;
    flashDuration = -1;
    bright();
}

// Flash for a finite duration (ms)
void LightManager::flashFor(long durationMs)
{
    flashDuration = durationMs;
    startFlash();
}