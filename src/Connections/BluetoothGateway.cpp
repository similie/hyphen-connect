#include "connections/BluetoothGateway.h"

BluetoothGateway::BluetoothGateway(const char *name)
    : deviceName(name), connected(false) {}

// Initialize Bluetooth
bool BluetoothGateway::init()
{
    if (!btSerial.begin(deviceName))
    {
        Log.errorln("Failed to initialize Bluetooth");
        return false;
    }
    Log.noticeln("Bluetooth initialized successfully");
    return true;
}
/**
 * @brief implements the getTime method
 *
 * @param timeinfo
 * @return true
 * @return false
 */
bool BluetoothGateway::getTime(struct tm &timeinfo, float &timezone)
{
    return false;
}

// Connect to Bluetooth device (always returns true since ESP32 listens for incoming connections)
bool BluetoothGateway::connect()
{
    if (btSerial.hasClient())
    {
        connected = true;
        Log.noticeln("Bluetooth client connected");
    }
    else
    {
        Log.noticeln("Waiting for Bluetooth client...");
    }
    return connected;
}

// Disconnect Bluetooth client
void BluetoothGateway::disconnect()
{
    btSerial.end();
    connected = false;
    Log.noticeln("Bluetooth client disconnected");
}

// Check if Bluetooth client is connected
bool BluetoothGateway::isConnected()
{
    connected = btSerial.hasClient();
    return connected;
}

// Power on Bluetooth (reinitialize)
bool BluetoothGateway::on()
{
    return init();
}

// Power off Bluetooth
bool BluetoothGateway::off()
{
    disconnect();
    return true;
}

// Keep-alive for Bluetooth (retries until connected)
bool BluetoothGateway::keepAlive(uint8_t maxRetries)
{
    uint8_t attempts = 0;
    while (!connect() && attempts < maxRetries)
    {
        delay(1000);
        attempts++;
    }
    return isConnected();
}

// Maintain Bluetooth connection (reconnect if disconnected)
void BluetoothGateway::maintain()
{
    if (!isConnected())
    {
        connect();
    }
}

// Return nullptr since Bluetooth doesn't use a WiFi or network Client
Client *BluetoothGateway::getClient()
{
    return nullptr;
}

// Send configuration data over Bluetooth
void BluetoothGateway::sendConfigData(const String &configData)
{
    if (isConnected())
    {
        btSerial.println(configData);
        Log.noticeln("Sent config data over Bluetooth");
    }
    else
    {
        Log.noticeln("No Bluetooth client connected");
    }
}

// Receive data from Bluetooth
String BluetoothGateway::receiveData()
{
    String data;
    if (isConnected())
    {
        while (btSerial.available())
        {
            data += (char)btSerial.read();
        }
    }
    return data;
}