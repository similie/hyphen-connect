#ifndef __aws_mqtt_processor__
#define __aws_mqtt_processor__
#include <Ticker.h>
#include <PubSubClient.h>
#include "connections/Connection.h"
#include "managers/HealthCheck.h"
#include "Managers.h"
#include "Processor.h"
#include <freertos/semphr.h>
// #define free esp_mbedtls_mem_free

#include "mbedtls/platform.h"

#include <ArduinoJson.h>

extern const uint8_t _binary_src_certs_root_ca_pem_start[] asm("_binary_src_certs_root_ca_pem_start");
extern const uint8_t _binary_src_certs_root_ca_pem_end[] asm("_binary_src_certs_root_ca_pem_end");

extern const uint8_t _binary_src_certs_device_cert_pem_start[] asm("_binary_src_certs_device_cert_pem_start");
extern const uint8_t _binary_src_certs_device_cert_pem_end[] asm("_binary_src_certs_device_cert_pem_end");

extern const uint8_t _binary_src_certs_private_key_pem_start[] asm("_binary_src_certs_private_key_pem_start");
extern const uint8_t _binary_src_certs_private_key_pem_end[] asm("_binary_src_certs_private_key_pem_end");

#ifndef MQTT_KEEP_ALIVE_INTERVAL
#define MQTT_KEEP_ALIVE_INTERVAL 30 // seconds
#endif

#ifndef MQTT_KEEP_ALIVE_INTERVAL_LOOP_OFFSET
#define MQTT_KEEP_ALIVE_INTERVAL_LOOP_OFFSET 1 // seconds
#endif

#ifndef MQTT_CA_CERTIFICATE_NAME
#define MQTT_CA_CERTIFICATE_NAME "/root-ca.pem" // the root certificate for the mqtt connection
#endif

#ifndef MQTT_DEVICE_CERTIFICATE_NAME
#define MQTT_DEVICE_CERTIFICATE_NAME "/device-cert.pem" // the root certificate for the mqtt connection
#endif

#ifndef MQTT_DEVICE_PRIVATE_KEY_NAME
#define MQTT_DEVICE_PRIVATE_KEY_NAME "/private-key.pem" // the root certificate for the mqtt connection
#endif

#define CERT_LENGTH 3 // should not be changed

class SecureMQTTProcessor : public Processor
{
public:
    SecureMQTTProcessor(Connection &connection);
    bool connect();
    void disconnect();
    bool isConnected();
    bool publish(const char *, const char *);
    bool publish(const char *topic, uint8_t *, size_t);
    bool subscribe(const char *, std::function<void(const char *, const char *)>);
    bool unsubscribe(const char *);
    bool init();
    bool maintain();
    void loop();
    bool ready();

private:
    volatile bool processing = false;
    volatile bool loopEvent = false;
    volatile bool maintenanceEvent = false;
    volatile bool MQTTConnected = false;
    HealthCheck health;
    bool waitForMaintenance();
    bool preProcesss();
    bool loopNotReady();
    struct Lock
    {
        Lock() { xSemaphoreTakeRecursive(mutex(), portMAX_DELAY); }
        ~Lock() { xSemaphoreGiveRecursive(mutex()); }
        static SemaphoreHandle_t &mutex()
        {
            static SemaphoreHandle_t m = xSemaphoreCreateRecursiveMutex();
            return m;
        }
    };
    String certificates[CERT_LENGTH];
    bool certsCached = false;
    enum cachedCertificates
    {
        CA,
        DeviceCertificate,
        DevicePrivateKey
    };
    bool reconnect();
    bool onDisconnect();
    // bool hardDisconnect();
    bool attachClients();
    bool attachServer();
    bool attachCertificates();
    bool initialized = false;
    FileManager fm;
    LightManager light;
    TaskHandle_t maintainConnectHandle = NULL;
    uint8_t connectCount = 0;
    const uint16_t KEEP_ALIVE = MQTT_KEEP_ALIVE_INTERVAL;                                              // Keep-alive interval in seconds
    const unsigned int KEEP_ALIVE_INTERVAL = KEEP_ALIVE * MQTT_KEEP_ALIVE_INTERVAL_LOOP_OFFSET * 1000; // Keep-alive interval in seconds
    const char *CLIENT_ID = DEVICE_PUBLIC_ID;
    bool keepAliveReady();
    const uint8_t MAX_CONNECTION_ATTEMPTS = 5;
    Connection &connection;
    PubSubClient mqttClient;
#ifndef INSECURE_MQTT
    SecureClient *secureClient = nullptr;
#else
    Client *client = nullptr;
#endif
    void stop();
    bool cleanupDisconnect();
    void restartSSL();
    void mqttCallback(char *topic, byte *payload, unsigned int length);
    bool setupSecureConnection();
    bool loadCertificates();
    bool connectServer();
    int hasSubscription();
    void subscribeToTopics();
    bool subscribeToTopic(const char *topic);
    bool unsubscribeToTopic(const char *topic);
    bool runMaintenance();
    void threadConnectionMaintenance();
    static void maintenanceCallback(SecureMQTTProcessor *instance);
    static void threadConnectionMaintenance(void *pv);
    void toggleMaintenance();
    bool isMaintenanceRunning = false;
    // bool maintaining = false;
    const size_t restorationAttempts = 5;
    Ticker _keepAliveTicker;
    TaskHandle_t maintenceHandle = nullptr;
    esp_timer_handle_t _maintenanceTimer;
    void stopMaintenanceTicker();
    void setMaintenanceTicker();
    void resetMaintenanceTicker();
    template <class TArg>
    void attach(unsigned long seconds, void (*callback)(TArg), TArg arg);
};

#endif