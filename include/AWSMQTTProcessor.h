

#ifndef __aws_mqtt_processor__
#define __aws_mqtt_processor__

#include <PubSubClient.h>
// #include "cellular.h"
#include "Connection.h"
// #include "certificates.h"
#include "WiFi.h"
#include <TinyGsmClient.h> // Include this for secure cellular connections
#include <WiFiClientSecure.h>
#include "FS.h"     // File system for SPIFFS
#include "SPIFFS.h" // ESP32 SPIFFS
// AWS IoT Core details

#define THREAD_MODE 0

class AWSMQTTProcessor
{
public:
    AWSMQTTProcessor(Connection *connection);
    bool connect();
    void disconnect();
    bool isConnected();

    bool publish(const char *topic, const char *payload);
    bool subscribe(const char *topic);
    bool init();
    void process();
    void loop();

    void setThread();
    static void maintainConnection(void *param);
    TaskHandle_t maintainConnectHandle = NULL;
    unsigned long lastInActivity = 0;
    const unsigned int KEEP_ALIVE_INTERVAL = 30 * 1000; // Keep-alive interval in seconds
private:
    const char *AWS_IOT_ENDPOINT = "a2hreerobwhgvz-ats.iot.us-east-1.amazonaws.com"; // AWS IoT Core endpoint //"100.64.100.1"; //
    const int AWS_IOT_PORT = 8883;
    const char *AWS_CLIENT_ID = DEVICE_PUBLIC_ID; // "2e11908790c75fb5915953d7";
    bool keepAliveReady();
    void loopMaintain();
    Connection *connection;
    // Cellular *cellular;
    // TinyGsmClient *gsmClient;
    PubSubClient mqttClient;
    // SSLClientESP32 sslClient;
    void mqttCallback(char *topic, byte *payload, unsigned int length);
    bool setupAWSConnection();
    bool loadCertificates();
    bool connectServer();
    std::function<void(char *, byte *, unsigned int)> mqttCallbackFunction;

    // void mqttCallback(char *topic, byte *payload, unsigned int length);
};

#endif