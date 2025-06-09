#include "managers/HealthCheck.h"

void HealthCheck::off()
{
    isOn = false;
}

void HealthCheck::on()
{
    isOn = true;
    lastLoop = millis();
}

void HealthCheck::interrogate()
{
    if (!isOn)
        return;

    unsigned long now = millis();
    unsigned long elapsed = now - lastLoop;
    Serial.printf("HealthCheck: %lu ms since last keep alive\n", elapsed);
    if (elapsed > MAINTENANCE_INTERVAL)
    {
        return reboot();
    }
    lastLoop = now;
}
void HealthCheck::loop()
{
    if (!isOn)
        return;
    lastLoop = millis();
}

void HealthCheck::reboot()
{
    // Implement the reboot logic here
    // For example, you can use ESP.restart() if using ESP32
    Serial.println("Rebooting due to health check failure...");
    delay(1000); // Optional delay before reboot
    ESP.restart();
}