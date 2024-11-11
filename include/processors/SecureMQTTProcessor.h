

#ifndef __aws_mqtt_processor__
#define __aws_mqtt_processor__
#include <Ticker.h>
#include <PubSubClient.h>
#include "FS.h"     // File system for SPIFFS
#include "SPIFFS.h" // ESP32 SPIFFS
#include "connections/Connection.h"
#include "managers/CommunicationRegistry.h"
#include "processors/Processor.h"
#include "managers/LightManager.h"
class SecureMQTTProcessor : public Processor
{
public:
    SecureMQTTProcessor(Connection &connection);
    bool connect();
    void disconnect();
    bool isConnected();
    bool publish(const char *topic, const char *payload);
    bool subscribe(const char *topic, std::function<void(const char *, const char *)> callback);
    bool init();
    void loop();

private:
    void reconnect();
    bool hardDisconnect();
    LightManager light;
    TaskHandle_t maintainConnectHandle = NULL;
    uint8_t connectCount = 0;
    const uint8_t HARD_CONNECTION_RESTART = 10;
    unsigned long lastInActivity = 0;
    const uint8_t KEEP_ALIVE = 30;                              // Keep-alive interval in seconds
    const unsigned int KEEP_ALIVE_INTERVAL = KEEP_ALIVE * 1000; // Keep-alive interval in seconds
    const char *AWS_CLIENT_ID = DEVICE_PUBLIC_ID;
    bool keepAliveReady();
    Connection &connection;
    PubSubClient mqttClient;
    SSLClientESP32 sslClient;
    void mqttCallback(char *topic, byte *payload, unsigned int length);
    bool setupAWSConnection();
    bool loadCertificates();
    bool connectServer();
    int hasSubscription();
    void subscribeToTopics();
    bool subscribeToTopic(const char *topic);
    static void maintenanceCallback(SecureMQTTProcessor *instance);
};

#endif