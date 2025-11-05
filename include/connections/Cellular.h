
#include <Ticker.h>
#include "connections/Connection.h"
#define SerialMon Serial
#define SerialAT Serial1
#include <TinyGsmClient.h>
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
#endif
#include "mbedtls/platform.h"
#include "mbedtls/ssl.h"          // for mbedtls_ssl_session_reset()
#include "mbedtls/ssl_internal.h" // for accessing internal structs if needed
#include <freertos/semphr.h>
#include "SSLClientESP32.h"
#define SSLClient SSLClientESP32

#ifndef cellular_h
#define cellular_h

#ifndef NETWORK_MODE
NETWORK_MODE 2
#endif

#ifndef GSM_SIM_PIN
#define GSM_SIM_PIN ""
#endif

    enum class SimType {
        SIM7070,
        SIM7600
    };

typedef struct
{
    float lat;
    float lon;
    float speed;
    float alt;
} GPSData;

class CellularSecureClient : public SecureClient, private SSLClient
{
private:
    bool _stopped = true;

    // RAII helper that takes/releases a recursive mutex
    struct Lock
    {
        Lock() { xSemaphoreTakeRecursive(mutex(), portMAX_DELAY); }
        ~Lock() { xSemaphoreGiveRecursive(mutex()); }
    };

    // Singleton recursive mutex for all SSLClient calls
    static SemaphoreHandle_t &mutex()
    {
        static SemaphoreHandle_t m = xSemaphoreCreateRecursiveMutex();
        return m;
    }

public:
    CellularSecureClient() = default;
    ~CellularSecureClient() override = default;

    // Certificate methods
    void setCACert(const char *rootCA) override
    {
        Lock l;
        SSLClient::setCACert(rootCA);
    }
    void setCertificate(const char *clientCert) override
    {
        Lock l;
        SSLClient::setCertificate(clientCert);
    }
    void setPrivateKey(const char *privateKey) override
    {
        Lock l;
        SSLClient::setPrivateKey(privateKey);
    }
    void setCACertBundle(const uint8_t *bundle) override
    {
        Lock l;
        SSLClient::setCACertBundle(bundle);
    }

    // Underlying transport
    void setClient(Client *client) override
    {
        Lock l;
        SSLClient::setHandshakeTimeout(10000);
        SSLClient::setClient(client);
    }

    // PSK / insecure / verify / handshake timeout
    void setPreSharedKey(const char *id, const char *psk) override
    {
        Lock l;
        SSLClient::setPreSharedKey(id, psk);
    }
    void setInsecure() override
    {
        Lock l;
        SSLClient::setInsecure();
    }
    bool verify(const char *fp, const char *host) override
    {
        Lock l;
        return SSLClient::verify(fp, host);
    }
    void setHandshakeTimeout(unsigned long t) override
    {
        Lock l;
        SSLClient::setHandshakeTimeout(t);
    }

    // Connect: clears _stopped on success
    int connect(IPAddress ip, uint16_t port) override
    {
        Lock l;
        int rc = SSLClient::connect(ip, port);
        if (rc == 1)
            _stopped = false;
        return rc;
    }
    int connect(const char *host, uint16_t port) override
    {
        Lock l;
        int rc = SSLClient::connect(host, port);
        if (rc == 1)
            _stopped = false;
        return rc;
    }

    // Data I/O
    size_t write(uint8_t b) override
    {
        if (_stopped)
            return 0;
        Lock l;
        return SSLClient::write(b);
    }
    size_t write(const uint8_t *buf, size_t sz) override
    {
        if (_stopped)
            return 0;
        Lock l;
        return SSLClient::write(buf, sz);
    }
    int available() override
    {
        if (_stopped)
            return 0;
        Lock l;
        return SSLClient::available();
    }
    int read() override
    {
        if (_stopped)
            return -1;
        Lock l;
        return SSLClient::read();
    }
    int read(uint8_t *buf, size_t sz) override
    {
        if (_stopped)
            return -1;
        Lock l;
        return SSLClient::read(buf, sz);
    }
    int peek() override
    {
        if (_stopped)
            return -1;
        Lock l;
        return SSLClient::peek();
    }
    void flush() override
    {
        if (_stopped)
            return;
        Lock l;
        SSLClient::flush();
    }

    // Stop & connected checks
    void stop() override
    {
        Lock l;
        if (!_stopped)
        {
            SSLClient::stop();
            _stopped = true;
        }
    }
    uint8_t connected() override
    {
        if (_stopped)
            return 0;
        Lock l;
        return SSLClient::connected();
    }
    operator bool() override
    {
        Lock l;
        return SSLClient::operator bool();
    }
};

class Cellular : public Connection
{
public:
    Cellular();
    bool connect();
    void disconnect();
    bool clearCredentials();
    bool isConnected();
    SimType getActiveSim();
    void setActiveSim(SimType simType);
    bool on();
    bool off();
    bool keepAlive(uint8_t seconds);
    void restore() override;
    bool init();
    bool maintain();
    TinyGsm &getModem();
    Client &getClient() override;
    SecureClient &secureClient() override;
    Client &getNewClient() override;
    SecureClient &getNewSecureClient() override;
    bool enableGPS();
    bool disableGPS();
    bool getCellularTime(struct tm &);
    GPSData getGPSData();
    Connection &connection() override { return *this; }
    ConnectionClass getClass() { return ConnectionClass::CELLULAR; }
    bool getTime(struct tm &, float &) override;
    String getIMEI();
    String getIMSI();
    String getLocalIP();
    String getModemInfo();
    String getOperator();
    String getProvider();
    int16_t getNetworkMode();
    int16_t getSignalQuality();
    String getSimCCID();
    float getTemperature();
    bool setSimPin(const char *);
    bool setApn(const char *);
    bool reload();
    bool setFunctionality(int);
    bool powerSave(bool);
    bool factoryReset();

private:
#ifdef DUMP_AT_COMMANDS
    StreamDebugger debugger;
#endif
    String simPin = String(GSM_SIM_PIN);
    TinyGsmClient gsmClient;
    CellularSecureClient sslClient;
    uint8_t CELLULAR_CID = 1; // Connection ID for TinyGsmClient
    TinyGsm modem;
    Ticker tick;
    SimType activeSim;
    bool connected;
    String apn = String(CELLULAR_APN);
    void setClient();
    const char *gprsUser = "";
    const char *gprsPass = "";
    uint8_t keepAliveInterval; // Store the delay interval in seconds
    TaskHandle_t keepAliveHandle = NULL;
    TaskHandle_t maintainHandle = NULL;    // Task handle for maintain task
    static void maintainTask(void *param); // Task function for maintain
    uint32_t maintainIntervalMs = 5000;    // Interval for maintain checks (5 seconds, adjustable)
    uint8_t connectionAttempts = 0;
    const uint8_t maxConnectionAttempts = 5;
    bool initModem();
    void setupPower();
    bool setupNetwork();
    int getBuildYear();
    bool syncTimeViaCNTP(float tz);
    void setSimRegistration();
};
#endif