

#ifndef __aws_mqtt_processor__
#define __aws_mqtt_processor__
#include <Ticker.h>
#include <PubSubClient.h>
// #include "cellular.h"
#include "Connection.h"
#include "FS.h"     // File system for SPIFFS
#include "SPIFFS.h" // ESP32 SPIFFS
#include "CommunicationRegistry.h"
#include "functional"

#define THREAD_MODE 0

class AWSMQTTProcessor
{
public:
    AWSMQTTProcessor(Connection &connection);
    bool connect();
    void disconnect();
    bool isConnected();

    bool publish(const char *topic, const char *payload);
    bool subscribe(const char *topic, std::function<void(const char *, const char *)> callback);
    bool init();
    void process();
    void loop();
    void startMaintenanceLoop();
    // Method to register a callback
    bool registerCallback(std::function<void(const char *, const char *)> callback);
    // Function to trigger all callbacks
    void triggerCallbacks(const char *topic, const char *payload);
    void runMaintenanceConnection();

private:
    // void setThread();
    void reconnect();
    bool hardDisconnect();
    Ticker tick;
    // static void maintainConnection(void *param);
    TaskHandle_t maintainConnectHandle = NULL;
    uint8_t connectCount = 0;
    const uint8_t HARD_CONNECTION_RESTART = 10;
    unsigned long lastInActivity = 0;
    const unsigned int KEEP_ALIVE_INTERVAL = 30 * 1000; // Keep-alive interval in seconds
    // void destroyThread();
    const char *AWS_IOT_ENDPOINT = "a2hreerobwhgvz-ats.iot.us-east-1.amazonaws.com"; // AWS IoT Core endpoint //"100.64.100.1"; //
    const int AWS_IOT_PORT = 8883;
    const char *AWS_CLIENT_ID = DEVICE_PUBLIC_ID; // "2e11908790c75fb5915953d7";
    bool keepAliveReady();
    // void loopMaintain();
    Connection &connection;
    // Cellular cellular;
    Client *connectionClient;
    PubSubClient mqttClient;
    SSLClientESP32 sslClient;
    void mqttCallback(char *topic, byte *payload, unsigned int length);
    bool setupAWSConnection();
    bool loadCertificates();
    bool connectServer();
    int hasSubscription();
    void subscribeToTopics();
    bool subscribeToTopic(const char *topic);
    static void maintenanceCallback(AWSMQTTProcessor *instance);
    // void mqttCallback(char *topic, byte *payload, unsigned int length);
};

#endif