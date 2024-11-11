#ifndef ESP32WIFI_H
#define ESP32WIFI_H

#include <WiFi.h>
#include <Preferences.h>
#include "connections/Connection.h"
class WiFiConnection : public Connection
{
private:
    struct WiFiNetwork
    {
        char ssid[32];
        char password[64];
    };

    WiFiNetwork networks[10];
    int networkCount; // Number of networks stored
    const char *ssid;
    const char *password;
    bool connected = false;
    WiFiClient client; // Persistent WiFi client
    Preferences preferences;
    bool preferencesInitialized = false; // Track if preferences have been initialized
    void loadNetworks();                 // Load networks from storage
    void saveNetworks();                 // Save networks to storage

public:
    WiFiConnection();
    WiFiConnection(const char *ssid, const char *password);
    bool addNetwork(const char *ssid, const char *password);
    bool init() override;
    bool connect() override;
    void disconnect() override;
    bool isConnected() override;
    bool on() override;
    bool off() override;
    bool keepAlive(uint8_t maxRetries) override;
    void maintain() override;
    Client *getClient() override;
};

#endif // ESP32WIFI_H