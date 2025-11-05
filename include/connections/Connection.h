#include <ArduinoLog.h>
#include <Arduino.h>
#include "managers/CoreDelay.h"
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

class SecureClient : public Client
{
public:
    virtual ~SecureClient() = default;
    ///--- Certificate methods ---
    virtual void setCACert(const char *rootCA) = 0;
    virtual void setCertificate(const char *clientCert) = 0;
    virtual void setPrivateKey(const char *privateKey) = 0;
    // virtual void setBufferSizes(int, int) = 0;
    ///--- PSK (pre-shared key) methods ---
    virtual void setPreSharedKey(const char *identity, const char *psk) = 0;
    ///--- Insecure/no-verify mode ---
    virtual void setInsecure() = 0;

    virtual void setCACertBundle(const uint8_t *) = 0;
    ///--- Handshake timeout (ms) ---
    virtual void setHandshakeTimeout(unsigned long timeout) = 0;
    ///--- Fingerprint/domain verify ---
    virtual bool verify(const char *fingerprint,
                        const char *domainName = nullptr) = 0;
    // virtual bool verify(const char *fp, uint16_t port) = 0;
    virtual void setClient(Client *) = 0;
    // Allow connect() overload that configures certs inline
    using Client::connect;
    virtual int connect(const char *host, uint16_t port,
                        const char *rootCA,
                        const char *clientCert,
                        const char *privateKey)
    {
        setCACert(rootCA);
        setCertificate(clientCert);
        setPrivateKey(privateKey);
        return connect(host, port);
    }
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
    virtual bool maintain() = 0;
    virtual Client &getClient() = 0;
    virtual SecureClient &secureClient() = 0;
    virtual Client &getNewClient() = 0;
    virtual SecureClient &getNewSecureClient() = 0;
    virtual bool init() = 0;
    virtual ConnectionClass getClass() = 0;
    virtual bool getTime(struct tm &, float &) = 0;
    virtual bool powerSave(bool) = 0;
    virtual void restore() = 0;
    // Virtual Destructor
    virtual Connection &connection();
    virtual ~Connection() {}
};

class NoOpClient : public Client
{
public:
    NoOpClient() {}

    // --- Connection methods ---
    int connect(IPAddress, uint16_t) override { return 0; }
    int connect(const char *, uint16_t) override { return 0; }

    // --- Data send ---
    size_t write(uint8_t) override { return 0; }
    size_t write(const uint8_t *buf, size_t sz) override { return 0; }

    // --- Data receive ---
    int available() override { return 0; }
    int read() override { return -1; }
    int read(uint8_t *buf, size_t sz) override { return -1; }
    int peek() override { return -1; }

    // --- Stream control ---
    void flush() override {}
    void stop() override {}

    // --- Connection state ---
    uint8_t connected() override { return 0; } // `Client::connected()` returns uint8_t  [oai_citation_attribution:0‡GitHub](https://github.com/arduino/ArduinoCore-avr/blob/master/cores/arduino/Client.h)
    operator bool() override { return false; } // matches `virtual operator bool()`  [oai_citation_attribution:1‡GitHub](https://github.com/arduino/ArduinoCore-avr/blob/master/cores/arduino/Client.h)
};

#endif
