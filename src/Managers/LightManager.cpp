// File: LightManager.cpp

#include "managers/LightManager.h"

// Constructor: configure the PWM channel
LightManager::LightManager()
{
    ledcSetup(pwmChannel, pwmFrequency, pwmResolution);
    ledcAttachPin(LED_PIN, pwmChannel);
    off(); // Start with LED off
}

// Fade in/out over 8 ms per step
void LightManager::pinLoop()
{
    if (stopBreathing)
    {
        return;
    }
    for (int duty = 0; duty <= 255; ++duty)
    {
        ledcWrite(pwmChannel, duty);
        vTaskDelay(pdMS_TO_TICKS(8));
        if (stopBreathing)
        {
            break;
        }
    }

    if (stopBreathing)
    {
        return;
    }
    for (int duty = 255; duty >= 0; --duty)
    {
        ledcWrite(pwmChannel, duty);
        vTaskDelay(pdMS_TO_TICKS(8));
        if (stopBreathing)
        {
            break;
        }
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

void LightManager::processFlash(long durationMs)
{
    unsigned long start = millis();
    while (true)
    {
        flash();
        if (durationMs >= 0 &&
            millis() - start > (unsigned long)durationMs)
        {
            break;
        }
        coreDelay(100);
    }
}

// Task to run a timed flash loop
void LightManager::runFlash(void *param)
{
    LightManager *light = static_cast<LightManager *>(param);
    unsigned long start = millis();
    light->processFlash(light->flashDuration);
    light->bright();
    vTaskDelete(NULL);
    light->flashHandle = nullptr;
    light->flashDuration = -1;
}

// Task to run continuous breathing (flash + endless fade)
void LightManager::runBreathing(void *param)
{

    LightManager *light = static_cast<LightManager *>(param);
    light->stopBreathing = false; // clear any old request
    // first run a timed flash
    light->processFlash(3000);
    // now continuous breathing until asked to stop
    while (!light->stopBreathing)
    {
        light->pinLoop();
    }
    // cleanup: clear the handle so others know we’re gone
    light->breathHandle = nullptr;
    light->processFlash(1000);
    // make sure LED ends off (or at bright)
    light->off();
    // now delete *this* task
    vTaskDelete(NULL);
}

// Start the breathing effect on core 1 at prio tskIDLE+1
void LightManager::startBreathing()
{
    if (breathHandle != nullptr)
        return;

    xTaskCreatePinnedToCore(
        runBreathing,
        "RunBreathingTask",
        4096, // increased stack to 4 KB to prevent overflow
        this,
        tskIDLE_PRIORITY + 1,
        &breathHandle,
        1);
}

void LightManager::endBreathing()
{
    stopBreathing = true;
}

void LightManager::bright()
{
    ledcWrite(pwmChannel, 255);
}

void LightManager::dim()
{
    ledcWrite(pwmChannel, 128);
}

void LightManager::off()
{
    ledcWrite(pwmChannel, 0);
    // coreDelay(100);
    // digitalWrite(LED_PIN, LOW);
}

// Start a flashing task on core 1 at prio tskIDLE+1
void LightManager::startFlash()
{
    if (flashHandle)
        return;
    flashHandle = nullptr; // cleared in runFlash
    xTaskCreatePinnedToCore(
        runFlash,
        "RunFlashingTask",
        4096, // 4 KB stack for flash task
        this,
        tskIDLE_PRIORITY + 1,
        &flashHandle,
        0);
}

void LightManager::endFlash()
{
    if (flashHandle == nullptr)
        return;

    vTaskDelete(NULL);
    flashHandle = nullptr;
    flashDuration = -1;
    bright();
}

void LightManager::flashFor(long durationMs)
{
    flashDuration = durationMs;
    startFlash();
}
