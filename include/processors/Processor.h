#ifndef processor_h
#define processor_h

#include <Arduino.h>
#include "functional"
#include <ArduinoLog.h>

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
#ifndef CERT_LENGTH
#define CERT_LENGTH 3 // should not be changed
#endif

enum cachedCertificates
{
    CA,
    DeviceCertificate,
    DevicePrivateKey
};

class Processor
{
public:
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() = 0;
    virtual bool init() = 0;
    virtual void loop() = 0;
    virtual bool ready() = 0;
    virtual void maintain() = 0;
    virtual bool publish(const char *topic, const char *payload) = 0;
    virtual bool subscribe(const char *topic, std::function<void(const char *, const char *)> callback) = 0;
    virtual bool unsubscribe(const char *topic) = 0;
};

#endif