#include <ArduinoLog.h>
#include <Arduino.h>
#ifndef logging_managers_h
#define logging_managers_h
class LoggingManager
{
private:
    bool started = false;

public:
    void start(int);
};

#endif // LOG_H