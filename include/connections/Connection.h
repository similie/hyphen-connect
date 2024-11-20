#include <ArduinoLog.h>
#include <Arduino.h>
#include "SSLClientESP32.h"

#ifndef connection_h
#define connection_h

enum class ConnectionType
{
    WIFI_PREFERRED,
    CELLULAR_PREFERRED,
    WIFI_ONLY,
    CELLULAR_ONLY,
    NONE
};
enum ConnectionClass
{
    WIFI,
    CELLULAR,
    ETHERNET,
    BLUETOOTH,
    LoRaWAN,
    NONE
};

class Connection
{
public:
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() = 0;
    virtual bool on() = 0;
    virtual bool off() = 0;
    virtual bool keepAlive(uint8_t) = 0;
    virtual void maintain() = 0;
    virtual Client *getClient() = 0;
    virtual bool init() = 0;
    virtual ConnectionClass getClass() = 0;
    virtual bool getTime(struct tm &, float &) = 0;
    // Virtual Destructor
    virtual Connection &connection();
    virtual ~Connection() {}
};
#endif
