

#ifndef __aws_mqtt_processor__
#define __aws_mqtt_processor__
#include <Ticker.h>
#include <PubSubClient.h>
#include "connections/Connection.h"
#include "Managers.h"
#include "Processor.h"

#ifndef MQTT_CA_CERTIFICATE_NAME
#define MQTT_CA_CERTIFICATE_NAME "/root-ca.pem" // the root certificate for the mqtt connection
#endif

#ifndef MQTT_DEVICE_CERTIFICATE_NAME
#define MQTT_DEVICE_CERTIFICATE_NAME "/device-cert.pem" // the root certificate for the mqtt connection
#endif

#ifndef MQTT_DEVICE_PRIVATE_KEY_NAME
#define MQTT_DEVICE_PRIVATE_KEY_NAME "/private-key.pem" // the root certificate for the mqtt connection
#endif

#define CERT_LENGTH 3

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
    String certificates[CERT_LENGTH];
    bool certsCached = false;
    enum cachedCertificates
    {
        CA,
        DeviceCertificate,
        DevicePrivateKey
    };
    void reconnect();
    bool hardDisconnect();
    bool attachClients();
    bool attachServer();
    bool attachCertificates();
    FileManager fm;
    LightManager light;
    TaskHandle_t maintainConnectHandle = NULL;
    uint8_t connectCount = 0;
    const uint8_t HARD_CONNECTION_RESTART = 10;
    unsigned long lastInActivity = 0;
    const uint8_t KEEP_ALIVE = 30;                              // Keep-alive interval in seconds
    const unsigned int KEEP_ALIVE_INTERVAL = KEEP_ALIVE * 1000; // Keep-alive interval in seconds
    const char *CLIENT_ID = DEVICE_PUBLIC_ID;
    bool keepAliveReady();
    const uint8_t MAX_CONNECTION_ATTEMPTS = 5;
    Connection &connection;
    PubSubClient mqttClient;
    SSLClientESP32 sslClient;
    void mqttCallback(char *topic, byte *payload, unsigned int length);
    bool setupSecureConnection();
    bool loadCertificates();
    bool connectServer();
    int hasSubscription();
    void subscribeToTopics();
    bool subscribeToTopic(const char *topic);
    static void maintenanceCallback(SecureMQTTProcessor *instance);
};

#endif