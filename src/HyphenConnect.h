
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

#include <functional>
#include "Managers.h"
#include "Connections.h"
#include "Processors.h"

#ifdef HYPHEN_THREADED
#include "HyphenRunner.h"
#endif

class HyphenConnect
{
    friend class HyphenRunner;

public:
    HyphenConnect(ConnectionType mode = ConnectionType::CELLULAR_ONLY);

    bool setup(int logLevel = LOG_LEVEL_SILENT);
    void loop();
    bool ready();
    // These just forward directly to manager
    bool subscribe(const char *topic, std::function<void(const char *, const char *)> cb);
    bool unsubscribe(const char *topic);
    bool publishTopic(const String &topic, const String &payload);
    bool publishTopic(const char *, uint8_t *, size_t);
    void function(const char *name, std::function<int(const char *)> fn);
    void variable(const char *name, int *v);
    void variable(const char *name, long *v);
    void variable(const char *name, String *v);
    void variable(const char *name, double *v);
    bool isConnected();
    void disconnect();
    bool connectionOn();
    bool connectionOff();
    bool isOnline() { return connectedOn; };
    Client &getClient();
    ConnectionClass getConnectionClass();
    Connection &getConnection();
    SubscriptionManager &getSubscriptionManager();
    ConnectionManager &getConnectionManager();
    GPSData getLocation();
    // accessors…
private:
    ConnectionManager connection;
    SecureMQTTProcessor processor;
    SubscriptionManager manager;
    LoggingManager logger;
    bool connectedOn = false;
    bool initialSetup = false;
    int loggingLevel = 0;
#ifdef HYPHEN_THREADED
    bool rebuildingThread = false;
    unsigned long threadCheck = 0;
    const unsigned long THREAD_CHECK_INTERVAL = 10000 * 6; // 10 seconds
    bool threadCheckReady();
    HyphenRunner &runner = HyphenRunner::get();
#endif
};

#endif
