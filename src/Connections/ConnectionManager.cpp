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

void ConnectionManager::restore()
{
    if (!currentConnection)
    {
        return;
    }

    return currentConnection->restore();
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

    if (conn.init() && conn.isConnected())
    {
        currentConnection = &conn;
        return true;
    }
    return false;
}

bool ConnectionManager::init()
{
    Log.noticeln("Initializing connections...");
    // Initialize all connections
    currentConnection = nullptr;
    return connect();
}

bool ConnectionManager::connect()
{
    Log.noticeln("Attempting to connect via connection manager...");
    if (currentConnection != nullptr && currentConnection->isConnected())
    {
        return true;
    }
    else if (currentConnection != nullptr && !currentConnection->isConnected())
    {
        Log.noticeln("Current connection is not connected. Attempting to reconnect...");
        currentConnection->disconnect();
        currentConnection->off(); // Power off the current connection
        if (currentConnection->init())
        {
            Log.noticeln("Re-initialized current connection.");
            return true;
        }
        disconnect();
        coreDelay(5000);
    }

    size_t numConnections = connections.size();
    Log.warningln("Failed to connect. Attempting all connections... %d", numConnections);
    // size_t currentIndex = 0;
    for (auto &conn : connections)
    {
        if (attemptConnection(*conn.get()))
        {
            return true;
        }
        conn->disconnect();
        conn->off();
        coreDelay(1000);
        Log.errorln("Failed to connect. Powering off and trying next connection...");
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
    if (currentConnection && currentConnection->maintain())
    {
        Log.noticeln("Connection is still active.");
        return true;
    }
    disconnect();
    return connect();
}

bool ConnectionManager::maintain()
{
    Log.noticeln("Maintaining connection...");
    if (currentConnection && currentConnection->isConnected())
    {
        return true;
    }
    else
    {
        return reconnect();
    }
}

Client &ConnectionManager::getClient()
{
    if (currentConnection)
    {
        return currentConnection->getClient();
    }
    static NoOpClient dummy;
    return dummy;
}

SecureClient &ConnectionManager::secureClient()
{
    return currentConnection->secureClient();
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

bool ConnectionManager::powerSave(bool on)
{
    if (currentConnection)
    {
        return currentConnection->powerSave(on);
    }
    return false;
}

bool ConnectionManager::getTime(struct tm &timeinfo, float &timezone)
{
    if (currentConnection)
    {
        return currentConnection->getTime(timeinfo, timezone);
    }
    return false;
}

Connection &ConnectionManager::getConnection(ConnectionClass findConnection)
{
    for (auto &conn : connections)
    {
        if (conn->getClass() == findConnection)
        {
            return *conn;
        }
    }
    // As a fallback, if there is at least one connection, return the first one.
    if (!connections.empty())
    {
        return *connections[0];
    }

    // Otherwise, create a static dummy (this should never happen in a properly configured system)
    static Cellular dummy;
    return dummy;
}
GPSData ConnectionManager::getLocation()
{
    Cellular &cellConn = (Cellular &)getConnection(ConnectionClass::CELLULAR);
    return cellConn.getGPSData();
}
bool ConnectionManager::updateApn(const char *apn)
{
    Cellular &cellConn = (Cellular &)getConnection(ConnectionClass::CELLULAR);
    return cellConn.setApn(apn);
}
bool ConnectionManager::updateSimPin(const char *simPin)
{
    Cellular &cellConn = (Cellular &)getConnection(ConnectionClass::CELLULAR);
    return cellConn.setSimPin(simPin);
}
bool ConnectionManager::addWifiNetwork(const char *ssid, const char *password)
{
    WiFiConnection &cellConn = (WiFiConnection &)getConnection(ConnectionClass::WIFI);
    return cellConn.addNetwork(ssid, password);
}