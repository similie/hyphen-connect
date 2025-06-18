#pragma once
#include <Arduino.h>
#include <ESP.h>
#ifndef HEALTH_CHECK_STUCK_INTERVAL
#define HEALTH_CHECK_STUCK_INTERVAL 60 * 1000 * 10 // 10 seconds
#endif

class HealthCheck
{
public:
    HealthCheck() = default;
    ~HealthCheck() = default;
    void off();
    void on();
    void interrogate();
    void loop();
    void reboot();
    // void setKeepAlive(unsigned long interval);

private:
    const unsigned long MAINTENANCE_INTERVAL{HEALTH_CHECK_STUCK_INTERVAL}; // 60 seconds
    bool isOn = false;
    // bool tapped = false;
    // unsigned long lastKeepAlive = 0;
    unsigned long lastLoop = 0;
    // unsigned long lastMaintenance = 0;
};