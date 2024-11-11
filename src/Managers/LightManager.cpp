#include "managers/LightManager.h"

LightManager::LightManager()
{
    // Configure PWM
    ledcSetup(pwmChannel, pwmFrequency, pwmResolution);
    ledcAttachPin(LED_PIN, pwmChannel);
}

void LightManager::pinLoop()
{
    // Fade in
    for (int dutyCycle = 0; dutyCycle <= 255; dutyCycle++)
    {
        ledcWrite(pwmChannel, dutyCycle);
        vTaskDelay(8); // Adjust to control the speed of fading
    }
    // Fade out
    for (int dutyCycle = 255; dutyCycle >= 0; dutyCycle--)
    {
        ledcWrite(pwmChannel, dutyCycle);
        vTaskDelay(8);
    }
}

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

void LightManager::runFlash(void *param)
{
    LightManager *light = static_cast<LightManager *>(param);
    unsigned long start = millis();

    while (true)
    {
        light->flash();
        if (light->flashDuration != -1 && millis() - start > light->flashDuration)
        {
            light->endFlash();
            break;
        }
        vTaskDelay(100);
    }
}

void LightManager::runBreathing(void *param)
{
    LightManager *light = static_cast<LightManager *>(param);
    light->flashDuration = 3000;
    LightManager::runFlash(param);
    light->flashDuration = -1;
    while (true)
    {
        light->pinLoop();
    }
}

void LightManager::startBreathing()
{
    if (breathHandle != NULL)
    {
        return;
    }

    xTaskCreatePinnedToCore(runBreathing, "RunBreathingTask", 4096, this, 1, &breathHandle, 1);
}
void LightManager::endBreathing()
{
    if (breathHandle == NULL)
    {
        return;
    }
    vTaskDelete(breathHandle);
    breathHandle = NULL;
    flashFor(3000);
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
}

void LightManager::startFlash()
{
    if (flashHandle != NULL)
    {
        return;
    }
    xTaskCreatePinnedToCore(runFlash, "RunFlashingTask", 4096, this, 1, &flashHandle, 1);
}
void LightManager::endFlash()
{
    if (flashHandle == NULL)
    {
        return;
    }
    vTaskDelete(flashHandle);
    flashHandle = NULL;
    flashDuration = -1;
    bright();
}
void LightManager::flashFor(long durationMs)
{
    flashDuration = durationMs;
    startFlash();
}