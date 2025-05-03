#ifndef processor_h
#define processor_h

#include <Arduino.h>
#include "functional"
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
    virtual bool unsubscribe(const char *topic) = 0;
};

#endif