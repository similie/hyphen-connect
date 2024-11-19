#include "connections/ConnectionManager.h"

ConnectionManager::ConnectionManager(ConnectionType type)
    : preferredType(type), currentConnection(nullptr)
{
    // Initialize connections based on preference
    switch (preferredType)
    {
    case ConnectionType::WIFI_PREFERRED:
        setWiFi();
        setCellular();
        break;
    case ConnectionType::CELLULAR_PREFERRED:
        setCellular();
        setWiFi();
        break;
    case ConnectionType::WIFI_ONLY:
        setWiFi();
        break;
    case ConnectionType::CELLULAR_ONLY:
        setCellular();
        break;
    }
}

ConnectionManager::~ConnectionManager()
{
    disconnect();
}

void ConnectionManager::setCellular()
{
    connections.push_back(make_unique<Cellular>());
}

void ConnectionManager::setWiFi()
{
    if (DEFAULT_WIFI_SSID && DEFAULT_WIFI_PASS)
    {
        connections.push_back(make_unique<WiFiConnection>(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS));
    }
    else
    {
        connections.push_back(make_unique<WiFiConnection>());
    }
}

bool ConnectionManager::attemptConnection(Connection &conn)
{
    if (conn.init() && conn.connect())
    {
        currentConnection = &conn;
        return true;
    }
    return false;
}

bool ConnectionManager::init()
{
    // Initialize all connections
    currentConnection = nullptr;
    for (auto &conn : connections)
    {
        if (!conn->init())
        {
            Log.errorln(
                "Failed to initialize connection. Powering off and trying next connection...");
            conn->off();
            delay(5000);
        }
        else
        {
            Log.noticeln("Successfully Established Connection");
            currentConnection = conn.get();
            return true;
        }
    }
    return false;
}

bool ConnectionManager::connect()
{

    if (currentConnection && currentConnection->isConnected())
    {
        return true;
    }
    else if (currentConnection && !currentConnection->isConnected())
    {
        if (currentConnection->connect())
        {
            return true;
        }
    }

    for (auto &conn : connections)
    {
        if (currentConnection)
        {
            disconnect();
        }
        if (attemptConnection(*conn))
        {
            return true;
        }
    }
    return false;
}

void ConnectionManager::disconnect()
{
    if (!currentConnection)
    {
        return;
    }
    currentConnection->disconnect();
    currentConnection->off(); // Power off the current connection
    currentConnection = nullptr;
}

bool ConnectionManager::isConnected()
{
    return currentConnection && currentConnection->isConnected();
}

bool ConnectionManager::on()
{
    // Power on all connections
    for (auto &conn : connections)
    {
        if (!conn->on())
        {
            return false;
        }
    }
    return true;
}

bool ConnectionManager::off()
{
    // Power off all connections
    for (auto &conn : connections)
    {
        if (!conn->off())
        {
            return false;
        }
    }
    return true;
}

bool ConnectionManager::keepAlive(uint8_t maxRetries)
{
    if (currentConnection)
    {
        return currentConnection->keepAlive(maxRetries);
    }
    return false;
}

bool ConnectionManager::reconnect()
{
    disconnect();
    return connect();
}

void ConnectionManager::maintain()
{
    if (currentConnection && currentConnection->isConnected())
    {
        currentConnection->maintain();
    }
    else
    {
        reconnect();
    }
}

Client *ConnectionManager::getClient()
{
    if (currentConnection)
    {
        return currentConnection->getClient();
    }
    return nullptr;
}

Connection &ConnectionManager::connection()
{
    if (currentConnection)
    {
        return currentConnection->connection();
    }
    return *this;
}

ConnectionClass ConnectionManager::getClass()
{
    if (currentConnection)
    {
        return currentConnection->getClass();
    }
    return ConnectionClass::NONE;
}

bool ConnectionManager::getTime(struct tm &timeinfo, float &timezone)
{
    if (currentConnection)
    {
        return currentConnection->getTime(timeinfo, timezone);
    }
    return false;
}