#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

#include <vector>
#include <memory>
#include "connections/Connection.h"
#include "connections/WiFiConnection.h"
#include "connections/Cellular.h"
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
    ~ConnectionManager();

    bool init() override;
    bool connect() override;
    void disconnect() override;
    bool isConnected() override;
    bool on() override;
    bool off() override;
    bool keepAlive(uint8_t maxRetries) override;
    void maintain() override;
    Client *getClient() override;
    Connection &connection() override;
    ConnectionClass getClass() override;
    bool getTime(struct tm &, float &) override;
};

#endif // CONNECTIONMANAGER_H