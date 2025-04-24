
#ifndef __hyphen_connect_h
#define __hyphen_connect_h

#include <functional>
#include <Arduino.h>
#include "Managers.h"
#include "Connections.h"
#include "Processors.h"

#ifndef CELLULAR_APN
#define CELLULAR_APN "similie" // Your APN
#endif

#ifndef GSM_SIM_PIN
#define GSM_SIM_PIN "" // if you use a sim pin
#endif

#ifndef MQTT_IOT_ENDPOINT
#define MQTT_IOT_ENDPOINT "" // your endpoint
#endif

#ifndef MQTT_IOT_PORT
#define MQTT_IOT_PORT 8883 //
#endif

#ifndef DEVICE_PUBLIC_ID
#define DEVICE_PUBLIC_ID "" // this is a unique id for the device
#endif

#ifndef TINY_GSM_MODEM_SIM7600
#define TINY_GSM_MODEM_SIM7600
#endif

#ifndef LED_PIN
#define LED_PIN 12 // default for test connection
#endif

#ifndef UART_BAUD
#define UART_BAUD 115200 // cellular debugging baud rate
#endif

#ifndef CELLULAR_PIN_TX
#define CELLULAR_PIN_TX 27 // tx pin for cellular connection
#endif

#ifndef CELLULAR_PIN_RX
#define CELLULAR_PIN_RX 26
#endif

#ifndef CELLULAR_POWER_PIN_AUX
#define CELLULAR_POWER_PIN_AUX 4
#endif

#ifndef CELLULAR_POWER_PIN
#define CELLULAR_POWER_PIN 25
#endif

#ifndef CELLULAR_IND_PIN
#define CELLULAR_IND_PIN 36
#endif

#ifndef DEFAULT_WIFI_SSID
#define DEFAULT_WIFI_SSID "" // your wifi ssid
#endif

#ifndef DEFAULT_WIFI_PASS
#define DEFAULT_WIFI_PASS "" // your wifi password. Keep safe
#endif

#ifndef MQTT_TOPIC_BASE
#define MQTT_TOPIC_BASE "HY/" // the base topic for the mqtt messages. This is used to filter messages
#endif

#ifdef HYPHEN_THREADED
#include "HyphenRunner.h"
#endif

class HyphenConnect
{
    // allow runner to touch private members:
    friend class HyphenRunner;

public:
    HyphenConnect(ConnectionType mode = ConnectionType::CELLULAR_ONLY);

    // your public API unchanged
    bool setup(int logLevel = LOG_LEVEL_SILENT);
    void loop();
    bool subscribe(const char *t, std::function<void(const char *, const char *)> cb);
    bool publishTopic(const String &topic, const String &payload);
    void function(const char *name, std::function<int(const char *)> fn);
    void variable(const char *name, int *v);
    void variable(const char *name, long *v);
    void variable(const char *name, String *v);
    void variable(const char *name, double *v);
    bool isConnected();
    void disconnect();

    // accessors
    Client &getClient();
    ConnectionClass getConnectionClass();
    Connection &getConnection();
    SubscriptionManager &getSubscriptionManager();
    ConnectionManager &getConnectionManager();

private:
    ConnectionManager connection;
    SecureMQTTProcessor processor;
    SubscriptionManager manager;
    LoggingManager logger;
    bool initialSetup = false;

#ifdef HYPHEN_THREADED
    HyphenRunner &runner;
#endif
};

#endif // __HYPHEN_CONNECT_H
       // class HyphenConnect
       // {
       // private:
       //     ConnectionManager connection;
       //     SecureMQTTProcessor processor;
       //     SubscriptionManager manager;
       //     bool initialSetup = false;
       //     LoggingManager logger;

// public:
//     HyphenConnect();
//     HyphenConnect(ConnectionType);
//     bool setup();
//     bool setup(int);
//     void loop();
//     bool subscribe(const char *, std::function<void(const char *, const char *)>);
//     bool publishTopic(String, String);
//     bool isConnected();
//     void disconnect();
//     void variable(const char *, int *);
//     void variable(const char *, long *);
//     void variable(const char *, String *);
//     void variable(const char *, double *);
//     void function(const char *, std::function<int(const char *)>);

//     Client &getClient();
//     ConnectionClass getConnectionClass();
//     Connection &getConnection();
//     SubscriptionManager &getSubscriptionManager();
//     ConnectionManager &getConnectionManager();
// };

// #endif