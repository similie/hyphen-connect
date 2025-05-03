
#include <Ticker.h>
#include "connections/Connection.h"
#include "SSLClientESP32.h"
// #include <ESP_SSLClient.h>
#define SerialMon Serial
#define SerialAT Serial1
#include <TinyGsmClient.h>
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
#endif

#include "mbedtls/ssl.h"          // for mbedtls_ssl_session_reset()
#include "mbedtls/ssl_internal.h" // for accessing internal structs if needed

#ifndef cellular_h
#define cellular_h

#ifndef NETWORK_MODE
NETWORK_MODE 2
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

class CellularSecureClient : public SecureClient, private SSLClientESP32
{
public:
    CellularSecureClient() = default;
    ~CellularSecureClient() override = default;

    // Certificate methods
    void setCACert(const char *rootCA) override
    {
        SSLClientESP32::setCACert(rootCA);
    }
    void setCertificate(const char *clientCert) override
    {
        SSLClientESP32::setCertificate(clientCert);
    }
    void setPrivateKey(const char *privateKey) override
    {
        SSLClientESP32::setPrivateKey(privateKey);
    }
    void setCACertBundle(const uint8_t *bundle) override
    {
        SSLClientESP32::setCACertBundle(bundle);
    }
    void setClient(Client *client) override
    {
        SSLClientESP32::setClient(client);
    }
    // PSK
    void setPreSharedKey(const char *identity, const char *psk) override
    {
        SSLClientESP32::setPreSharedKey(identity, psk);
    }
    // Insecure mode
    void setInsecure() override
    {
        SSLClientESP32::setInsecure();
    }
    // Handshake timeout
    void setHandshakeTimeout(unsigned long t) override
    {
        SSLClientESP32::setHandshakeTimeout(t);
    }
    // Fingerprint/domain verify
    bool verify(const char *fingerprint,
                const char *domainName) override
    {
        return SSLClientESP32::verify(fingerprint, domainName);
    }
    // Client interface implementation
    int connect(IPAddress ip, uint16_t port) override
    {
        return SSLClientESP32::connect(ip, port);
    }
    int connect(const char *host, uint16_t port) override
    {
        return SSLClientESP32::connect(host, port);
    }
    size_t write(uint8_t b) override { return SSLClientESP32::write(b); }
    size_t write(const uint8_t *buf, size_t size) override
    {
        return SSLClientESP32::write(buf, size);
    }
    int available() override { return SSLClientESP32::available(); }
    int read() override { return SSLClientESP32::read(); }
    int read(uint8_t *buf, size_t size) override
    {
        return SSLClientESP32::read(buf, size);
    }
    int peek() override { return SSLClientESP32::peek(); }
    void flush() override { SSLClientESP32::flush(); }
    void stop() override { SSLClientESP32::stop(); }
    uint8_t connected() override { return SSLClientESP32::connected(); }
    operator bool() override { return SSLClientESP32::operator bool(); }
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
    bool enableGPS();
    bool disableGPS();
    bool getCellularTime(struct tm &);
    GPSData getGPSData();
    Connection &connection() override { return *this; }
    ConnectionClass getClass() { return ConnectionClass::CELLULAR; }
    bool getTime(struct tm &, float &) override;
    inline String getIMEI();
    inline String getIMSI();
    inline String getLocalIP();
    inline String getModemInfo();
    inline String getOperator();
    inline String getProvider();
    inline int16_t getNetworkMode();
    inline int16_t getSignalQuality();
    inline String getSimCCID();
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
    // TinyGsmClient gsmClient;
    TinyGsmClient *gsmClient = nullptr;
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
    uint8_t keepAliveInterval;              // Store the delay interval in seconds
    static void keepAliveTask(void *param); // FreeRTOS task for keeping alive
    TaskHandle_t keepAliveHandle = NULL;
    TaskHandle_t maintainHandle = NULL;    // Task handle for maintain task
    static void maintainTask(void *param); // Task function for maintain
    uint32_t maintainIntervalMs = 5000;    // Interval for maintain checks (5 seconds, adjustable)
    uint8_t connectionAttempts = 0;
    const uint8_t maxConnectionAttempts = 5;
    bool initModem();
    void setupPower();
    bool setupNetwork();
    void terminateThreads();
    int getBuildYear();
    bool syncTimeViaCNTP(float tz);
    void setSimRegistration();
};
#endif