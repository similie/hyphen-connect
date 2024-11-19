#ifndef BLUETOOTHGATEWAY_H
#define BLUETOOTHGATEWAY_H

#include <BluetoothSerial.h>
#include "connections/Connection.h"

class BluetoothGateway : public Connection
{
private:
    BluetoothSerial btSerial;
    String deviceName;
    bool connected;

public:
    BluetoothGateway(const char *name);

    bool init() override;
    bool connect() override;
    void disconnect() override;
    bool isConnected() override;
    bool on() override;
    bool off() override;
    bool keepAlive(uint8_t maxRetries) override;
    void maintain() override;
    Client *getClient() override; // Return nullptr since Bluetooth doesn't use a WiFi client
    ConnectionClass getClass() { return ConnectionClass::BLUETOOTH; }

    // Additional methods to handle data exchange
    void sendConfigData(const String &configData);
    String receiveData();
    Connection &connection() override { return *this; }
    bool getTime(struct tm &, float &) override;
};

#endif // BLUETOOTHGATEWAY_H