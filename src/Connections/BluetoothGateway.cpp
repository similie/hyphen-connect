#include "connections/BluetoothGateway.h"

BluetoothGateway::BluetoothGateway(const char *name)
    : deviceName(name), connected(false) {}

// Initialize Bluetooth
bool BluetoothGateway::init()
{
    if (!btSerial.begin(deviceName))
    {
        Serial.println("Failed to initialize Bluetooth");
        return false;
    }
    Serial.println("Bluetooth initialized successfully");
    return true;
}

// Connect to Bluetooth device (always returns true since ESP32 listens for incoming connections)
bool BluetoothGateway::connect()
{
    if (btSerial.hasClient())
    {
        connected = true;
        Serial.println("Bluetooth client connected");
    }
    else
    {
        Serial.println("Waiting for Bluetooth client...");
    }
    return connected;
}

// Disconnect Bluetooth client
void BluetoothGateway::disconnect()
{
    btSerial.end();
    connected = false;
    Serial.println("Bluetooth client disconnected");
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
        Serial.println("Sent config data over Bluetooth");
    }
    else
    {
        Serial.println("No Bluetooth client connected");
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