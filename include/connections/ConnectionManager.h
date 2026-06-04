#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

#include <vector>
#include <memory>
#include "connections/Connection.h"
#ifndef HYPHEN_NATIVE_TEST
// Concrete transports pull in WiFi.h / TinyGSM / mbedtls, which only exist on
// the ESP32 target. Native unit tests inject FakeConnections instead and never
// touch these, so they're excluded from the host build.
#include "connections/WiFiConnection.h"
#include "connections/Cellular.h"
#endif
#include <algorithm>

template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args &&...args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

class ConnectionManager : public Connection
{
private:
    std::vector<std::unique_ptr<Connection>> connections;
    ConnectionType preferredType;
    Connection *currentConnection;
    bool reconnect();
    bool attemptConnection(Connection &conn);
    void setWiFi();
    void setCellular();

public:
    ConnectionManager(ConnectionType type);
    // Test/advanced ctor: inject the ordered connection list directly instead of
    // having the manager construct concrete WiFi/Cellular transports. Lets the
    // failover/recovery logic be exercised against fakes on the host.
    ConnectionManager(std::vector<std::unique_ptr<Connection>> conns, ConnectionType type);
    ~ConnectionManager();
    void restore() override;
    bool init() override;
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
    Connection &connection() override;
    ConnectionClass getClass() override;
    bool getTime(struct tm &, float &) override;
    bool powerSave(bool) override;
#ifndef HYPHEN_NATIVE_TEST
    // Cellular/WiFi-specific helpers depend on the concrete transport types
    // (GPSData, Cellular&, WiFiConnection&) and are excluded from the host build.
    GPSData getLocation();
    Connection &getConnection(ConnectionClass);
    bool updateApn(const char *);
    bool updateSimPin(const char *);
    bool addWifiNetwork(const char *, const char *);
#endif
};

#endif // CONNECTIONMANAGER_H