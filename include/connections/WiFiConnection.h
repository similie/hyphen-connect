#ifndef ESP32WIFI_H
#define ESP32WIFI_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include "connections/Connection.h"
/**
 * @brief Concrete SecureClient using ESP32's WiFiClientSecure
 *
 * Simply forwards all SecureClient API calls to the underlying
 * WiFiClientSecure instance.
 */
class WiFiSecureClient : public SecureClient, private WiFiClientSecure
{
public:
    WiFiSecureClient() = default;
    ~WiFiSecureClient() override = default;
    // Certificate methods
    void setCACert(const char *rootCA) override
    {
        WiFiClientSecure::setCACert(rootCA);
    }
    void setCertificate(const char *clientCert) override
    {
        WiFiClientSecure::setCertificate(clientCert);
    }
    void setPrivateKey(const char *privateKey) override
    {
        WiFiClientSecure::setPrivateKey(privateKey);
    }

    void setClient(Client *client) override
    {
        // WiFiClientSecure::setClient(client);
    }

    // PSK
    void setPreSharedKey(const char *identity, const char *psk) override
    {
        WiFiClientSecure::setPreSharedKey(identity, psk);
    }

    // Insecure mode
    void setInsecure() override
    {
        WiFiClientSecure::setInsecure();
    }
    // Handshake timeout
    void setHandshakeTimeout(unsigned long t) override
    {
        WiFiClientSecure::setHandshakeTimeout(t);
    }
    // Fingerprint/domain verify
    bool verify(const char *fingerprint,
                const char *domainName) override
    {
        return WiFiClientSecure::verify(fingerprint, domainName);
    }

    void setCACertBundle(const uint8_t *bundle) override
    {
        WiFiClientSecure::setCACertBundle(bundle);
    }
    // Client interface implementation
    int connect(IPAddress ip, uint16_t port) override
    {
        return WiFiClientSecure::connect(ip, port);
    }
    int connect(const char *host, uint16_t port) override
    {
        return WiFiClientSecure::connect(host, port);
    }
    size_t write(uint8_t b) override { return WiFiClientSecure::write(b); }
    size_t write(const uint8_t *buf, size_t size) override
    {
        return WiFiClientSecure::write(buf, size);
    }
    int available() override { return WiFiClientSecure::available(); }
    int read() override { return WiFiClientSecure::read(); }
    int read(uint8_t *buf, size_t size) override
    {
        return WiFiClientSecure::read(buf, size);
    }
    int peek() override { return WiFiClientSecure::peek(); }
    void flush() override { WiFiClientSecure::flush(); }
    void stop() override { WiFiClientSecure::stop(); }
    uint8_t connected() override { return WiFiClientSecure::connected(); }
    operator bool() override { return WiFiClientSecure::operator bool(); }
};

struct NetworkInfo
{
    String ssid;
    String bssid;
    int32_t rssi;
    uint8_t channel;
    wifi_auth_mode_t encryption;
    IPAddress localIP;
    IPAddress gatewayIP;
    IPAddress subnetMask;
    IPAddress dns1;
    IPAddress dns2;
};

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
    WiFiClient client;                   // Persistent WiFi client
    WiFiSecureClient sslClient;          // Secure WiFi client
    WiFiClient secondaryClient;          // new secondary HTTP client
    WiFiSecureClient secondarySslClient; // new secondary HTTPS client
    Preferences preferences;
    bool preferencesInitialized = false; // Track if preferences have been initialized
    void loadNetworks();                 // Load networks from storage
    void saveNetworks();                 // Save networks to storage

public:
    WiFiConnection();
    WiFiConnection(const char *ssid, const char *password);
    bool addNetwork(const char *ssid, const char *password);
    bool init() override;
    void restore() override {};
    bool connect() override;
    void disconnect() override;
    bool isConnected() override;
    bool on() override;
    bool off() override;
    bool keepAlive(uint8_t maxRetries) override;
    bool maintain() override;
    Client &getClient() override;
    SecureClient &secureClient() override;
    Client &getNewClient() override;
    SecureClient &getNewSecureClient() override;
    Connection &connection() override { return *this; }
    ConnectionClass getClass() { return ConnectionClass::WIFI; }
    bool getTime(struct tm &, float &) override;
    bool powerSave(bool);
    NetworkInfo getNetworkInfo();
};

#endif // ESP32WIFI_H