#include "connections/WiFiConnection.h"

WiFiConnection::WiFiConnection() : networkCount(0), connected(false)
{
}

WiFiConnection::WiFiConnection(const char *ssid, const char *password) : WiFiConnection()
{
    this->ssid = ssid;
    this->password = password;
}

// Add a new network and save to preferences
bool WiFiConnection::addNetwork(const char *ssid, const char *password)
{

    Serial.printf("Adding network SSID: %s PASSWORD: %s\n", ssid, password);
    // Check if the network is already in the list
    for (int i = 0; i < networkCount; i++)
    {
        if (strcmp(networks[i].ssid, ssid) == 0)
        { // Check if SSID already exists
            strncpy(networks[i].password, password, sizeof(networks[i].password) - 1);
            saveNetworks(); // Save changes to preferences
            Serial.printf("Updated network %s\n", ssid);
            return true;
        }
    }

    // Add a new network if SSID does not exist and there's room in the array
    if (networkCount < 10)
    {
        strncpy(networks[networkCount].ssid, ssid, sizeof(networks[networkCount].ssid) - 1);
        strncpy(networks[networkCount].password, password, sizeof(networks[networkCount].password) - 1);
        networkCount++;
        saveNetworks(); // Save changes to preferences
        Serial.printf("Added new network %s\n", ssid);
        return true;
    }
    else
    {
        Serial.println("Network limit reached (10). Cannot add more networks.");
        return false;
    }
}

// Load WiFi networks from non-volatile storage
void WiFiConnection::loadNetworks()
{
    networkCount = preferences.getInt("networkCount", 0);
    Serial.printf("Loaded %d networks\n", networkCount);
    for (int i = 0; i < networkCount; i++)
    {
        String ssidKey = "ssid" + String(i);
        String passKey = "pass" + String(i);
        String ssid = preferences.getString(ssidKey.c_str(), "");
        String password = preferences.getString(passKey.c_str(), "");
        if (ssid.length() > 0 && password.length() > 0)
        {
            strncpy(networks[i].ssid, ssid.c_str(), sizeof(networks[i].ssid) - 1);
            strncpy(networks[i].password, password.c_str(), sizeof(networks[i].password) - 1);
        }
    }
}

// Save WiFi networks to non-volatile storage
void WiFiConnection::saveNetworks()
{
    preferences.putInt("networkCount", networkCount);
    for (int i = 0; i < networkCount; i++)
    {
        String ssidKey = "ssid" + String(i);
        String passKey = "pass" + String(i);
        preferences.putString(ssidKey.c_str(), networks[i].ssid);
        preferences.putString(passKey.c_str(), networks[i].password);
    }
}

// Initialize WiFi in station mode
bool WiFiConnection::init()
{
    Serial.println("Initializing WiFi...");
    if (!preferencesInitialized)
    {
        preferencesInitialized = true;
        preferences.begin("wifi_config", false);
        if (ssid && password)
        {
            addNetwork(ssid, password);
        }
    }
    on();
    loadNetworks();
    return connect();
}

// Connect to the first available network in the list
bool WiFiConnection::connect()
{
    Serial.println("Connecting to WiFi...");
    if (isConnected())
    {
        return true;
    }
    Serial.printf("We have these networks %d\n", networkCount);
    for (int i = 0; i < networkCount; i++)
    {
        Serial.printf("Connecting to %s\n", networks[i].ssid);

        WiFi.begin(networks[i].ssid, networks[i].password);
        unsigned long startAttemptTime = millis();
        // Wait up to 10 seconds for connection
        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000)
        {
            delay(100);
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            connected = true;
            Serial.printf("Connected to %s\n", networks[i].ssid);
            return true;
        }
        else
        {
            Serial.printf("Failed to connect to %s\n", networks[i].ssid);
        }
    }
    delay(10000);
    connected = false;
    return false; // Return false if no network could be connected to
}

// Disconnect from WiFi
void WiFiConnection::disconnect()
{
    WiFi.disconnect();
    connected = false;
}

// Check if WiFi is connected
bool WiFiConnection::isConnected()
{
    connected = WiFi.status() == WL_CONNECTED;
    return connected;
}

// Power on WiFi
bool WiFiConnection::on()
{
    WiFi.mode(WIFI_STA);
    return true;
}

// Power off WiFi
bool WiFiConnection::off()
{
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    connected = false;
    return true;
}

// Keep the connection alive by reconnecting if disconnected
bool WiFiConnection::keepAlive(uint8_t maxRetries)
{
    if (!isConnected())
    {
        uint8_t attempts = 0;
        while (!connect() && attempts < maxRetries)
        {
            attempts++;
            delay(1000); // Retry delay
        }
    }
    return isConnected();
}

// Maintain the connection (reconnect if disconnected)
void WiFiConnection::maintain()
{
    // if (!isConnected())
    // {
    //     connect();
    // }
}

// Return a pointer to the WiFi client
Client *WiFiConnection::getClient()
{
    return &client;
}