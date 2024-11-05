#include <Arduino.h>
#include <SSLClientESP32.h>
#ifndef connection_h
#define connection_h
class Connection
{
public:
    // Pure virtual functions (abstract methods)
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() = 0;
    virtual bool on() = 0;
    virtual bool off() = 0;
    virtual bool keepAlive(uint8_t) = 0;
    virtual void maintain() = 0;
    virtual SSLClientESP32 &getClient() = 0;
    virtual bool init() = 0;
    // virtual bool sendData(const std::string &data, const std::string &topic) = 0;
    // virtual bool receiveData(std::string &data) = 0;

    // Virtual Destructor
    virtual ~Connection() {}
};
#endif
