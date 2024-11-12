#ifndef _processor_h
#define _processor_h
#include "functional"
#include <Arduino.h>
#include <ArduinoLog.h>
class Processor
{
public:
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() = 0;
    virtual bool init() = 0;
    virtual void loop() = 0;
    virtual bool publish(const char *topic, const char *payload) = 0;
    virtual bool subscribe(const char *topic, std::function<void(const char *, const char *)> callback) = 0;
};

#endif