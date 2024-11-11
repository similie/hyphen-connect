#include <Arduino.h>

#ifndef LIGHT_MANAGER_H
#define LIGHT_MANAGER_H

class LightManager
{
private:
    const u_int8_t pwmChannel = 0;       // PWM channel
    const u_int16_t pwmFrequency = 5000; // PWM frequency in Hz
    const u_int8_t pwmResolution = 8;
    TaskHandle_t breathHandle = NULL; // PWM resolution (8 bits: 0-255)
    TaskHandle_t flashHandle = NULL;
    void pinLoop();
    static void runBreathing(void *param);
    static void runFlash(void *param);
    long flashDuration = -1;
    bool flashOn = false;
    void flash();

public:
    LightManager();
    void startBreathing();
    void endBreathing();
    void bright();
    void dim();
    void off();
    
    void startFlash();
    void endFlash();
    void flashFor(long durationMs);
};

#endif // LIGHT_MANAGER_H