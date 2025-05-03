#include "processors/SecureMQTTProcessor.h"
extern "C" void *esp_mbedtls_psram_calloc(size_t n, size_t s);
SecureMQTTProcessor::SecureMQTTProcessor(Connection &connection)
    : connection(connection)
{
    mbedtls_platform_set_calloc_free(esp_mbedtls_psram_calloc, free);
}

/**
 * @brief initialize the MQTT processor and the wireless connection
 *
 * @return true - if connected to the network
 */
bool SecureMQTTProcessor::init()
{

    if (!connection.isConnected())
    {
        return false;
    }
    // wait a tick for the network to be ready
    coreDelay(500);
    lastInActivity = millis();
    Log.noticeln("Network Connection Initialized");
    return connectServer();
}

bool SecureMQTTProcessor::attachClients()
{
    secureClient = &connection.secureClient();
    if (secureClient == nullptr)
    {
        Log.errorln("Secure client is set to null.");
        return false;
    }

    if (!loadCertificates())
    {
        Log.errorln("Failed to load certificates.");
        return false;
    }
    return true;
}

bool SecureMQTTProcessor::attachServer()

{

    if (secureClient == nullptr)
    {
        Log.errorln("Secure client is set to null.");
        return false;
    }
    // sslClient.validate(MQTT_IOT_ENDPOINT, MQTT_IOT_PORT);
    mqttClient
        .setServer(MQTT_IOT_ENDPOINT, MQTT_IOT_PORT)
        .setClient(*secureClient)
        .setKeepAlive(30)
        .setCallback([this](char *topic, byte *payload, unsigned int length)
                     { mqttCallback(topic, payload, length); });
    return true;
}

/**
 * @brief setup the connection to the MQTT server
 *
 * @return true - when the connection is established
 */
bool SecureMQTTProcessor::connectServer()
{
    // if (!connection.isConnected())
    // {
    //     Log.errorln("Network connection is not established.");
    //     return false;
    // }
    Log.noticeln("Connecting to MQTT server...");
    if (!attachClients())
    {
        return false;
    }
    Log.noticeln("CLients attached...");
    if (!attachServer())
    {
        return false;
    }

    Log.noticeln("Server Attached...");

    return connect();
}

/**
 * @brief returns true on an interval to keep the connection alive
 *
 * @return true - if the keep alive interval is ready
 */
bool SecureMQTTProcessor::keepAliveReady()
{
    if (millis() - lastInActivity > KEEP_ALIVE_INTERVAL && !reconnectingEvent)
    {
        lastInActivity = millis();
        return true;
    }
    return false;
}

void SecureMQTTProcessor::cleanupDisconnect()
{
    Log.errorln("Connection is not established.");
    reconnectingEvent = true;
    disconnect();
    // connection.maintain();
    lastInActivity = millis();
    reconnectingEvent = false;
}

/**
 * @brief runs on the loop and maintains the connection.
 * your main loop should not contain any blocking code
 */
void SecureMQTTProcessor::loop()
{
    // if we have never been through the inital setup, return,
    // it allows us to start the service when we are in threading mode
    // if (!initialized)
    // {
    //     return;
    // }
    // always loop the connected mqtt client
    // if (MQTTConnected)
    // {
    mqttClient.loop();
    // }

    // if we are not connected, we need to reconnect, we will check evern n seconds
    if (!keepAliveReady())
    {
        return;
    }
    Log.noticeln("Keep alive check");
    // if (!connection.isConnected())
    // {
    //     return cleanupDisconnect();
    // }
    reconnectingEvent = true;
    // maintain the connection
    if (!connection.maintain())
    {
        return cleanupDisconnect();
    }
    // check if the connection is still alive
    MQTTConnected = mqttClient.connected();
    Log.noticeln("MQTT CONNECTION STATUS %s", String(MQTTConnected));

    if (MQTTConnected)
    {
        lastInActivity = millis();
        reconnectingEvent = false;
        return;
    }
    // block the attemps in a multithreaded environment
    // reconnectingEvent = true;
    // light.endBreathing();
    Log.noticeln("Reconnecting to MQTT...");
    reconnect();
    lastInActivity = millis();
    reconnectingEvent = false;
}

bool SecureMQTTProcessor::attachCertificates()
{
    for (u_int8_t i = 0; i < CERT_LENGTH; i++)
    {
        if (certificates[i].isEmpty())
        {
            Log.errorln("Certificate is empty");
            return false;
        }
        const char *certContentC = certificates[i].c_str();
        // Serial.printf("Attaching certificate %d: %s\n", i, certContentC);
        switch (i)
        {
        case cachedCertificates::CA:
            secureClient->setCACert(certContentC);
            break;
        case cachedCertificates::DeviceCertificate:
            secureClient->setCertificate(certContentC);
            break;
        case cachedCertificates::DevicePrivateKey:
            secureClient->setPrivateKey(certContentC);
            break;
        default:
            break;
        }
    }
    return true;
}

/**
 * @brief loads the certificates from the secure MQTT connection
 *
 * @return true - all certificates are loaded
 */
bool SecureMQTTProcessor::loadCertificates()
{

    if (certsCached)
    {
        return attachCertificates();
    }

    if (!fm.start())
    {
        Log.errorln("Failed to initialize SPIFFS");
        return false;
    }
    coreDelay(200);
    // Load Root CA
#ifdef MQTT_CA_CERTIFICATE
    String caContent = String(MQTT_CA_CERTIFICATE);
#else
    String caContent = fm.loadFileContents(MQTT_CA_CERTIFICATE_NAME);
#endif

    if (caContent.isEmpty())
    {
        Log.errorln("CA file is empty");
        return false;
    }
    certificates[CA] = caContent;
    const char *caContentC = caContent.c_str();
    Log.noticeln("Loaded CA certificate:");
    Log.noticeln(caContent.substring(0, 20).c_str());
    coreDelay(100);
#ifdef MQTT_DEVICE_CERTIFICATE
    String certContent = String(MQTT_DEVICE_CERTIFICATE);
#else
    String certContent = fm.loadFileContents(MQTT_DEVICE_CERTIFICATE_NAME);
#endif
    if (certContent.isEmpty())
    {
        Log.errorln("Device certificate file is empty");
        return false;
    }

    certificates[DeviceCertificate] = certContent;
    const char *certContentC = certContent.c_str();
    Log.noticeln("Loaded device certificate:");
    Log.noticeln(certContent.substring(0, 20).c_str());
    coreDelay(100);
#ifdef MQTT_DEVICE_PRIVATE_KEY
    String keyContent = String(MQTT_DEVICE_PRIVATE_KEY);
#else
    String keyContent = fm.loadFileContents(MQTT_DEVICE_PRIVATE_KEY_NAME);
#endif

    if (keyContent.isEmpty())
    {
        Log.errorln("Private key file is empty");
        return false;
    }
    certificates[DevicePrivateKey] = keyContent;
    const char *keyContentC = keyContent.c_str();
    Log.noticeln("Loaded private key:");
    Log.noticeln(keyContent.substring(0, 20).c_str());
    fm.end();
    certsCached = true;
    return attachCertificates();
}

/**
 * @brief if the normal connection effort fails after a number of tries,
 * let's do a hard disconnect and restart the connection
 * @return true if successful, false otherwise
 */
// bool SecureMQTTProcessor::hardDisconnect()
// {
//     Log.noticeln("Restarting connection with a hard disconnect...");
//     connectCount = 0;
//     coreDelay(100);
//     connection.disconnect();
//     return true;
// }

void SecureMQTTProcessor::restartSSL()
{
    Log.noticeln("Restarting SSL connection...");
    if (secureClient == nullptr)
    {
        Log.noticeln("Secure client is null");
        return;
        // secureClient->reset(); // reset internal mbedTLS state
        // delete secureClient;
    }
    secureClient->flush();
    secureClient->stop();
    // if (secureClient->connected())
    // {
    //     secureClient->flush();
    //     secureClient->stop();
    // }
    // secureClient->flush();
    // secureClient->stop();
    coreDelay(100); // give hardware time
    // secureClient->reset(); // reset internal mbedTLS state: CRASHES SYSTEM
    secureClient = nullptr;
}

bool SecureMQTTProcessor::connect()
{
    // if (!connection.isConnected())
    // {
    //     connectCount = 0;
    //     Log.noticeln("Waiting for cellular connection...");
    //     return false;
    // }
    connectCount++;
    return setupSecureConnection();
}

/**
 * @brief connects to the MQTT server using the client id
 * @return true - if the connection is successful
 */
bool SecureMQTTProcessor::setupSecureConnection()
{
    Log.notice("Setting up IoT connection... ");
    Log.noticeln(CLIENT_ID);
    uint8_t i = 0;
    for (i; i <= MAX_CONNECTION_ATTEMPTS; i++)
    {
        if (!connection.isConnected())
        {
            return false;
        }

        if (mqttClient.connect(CLIENT_ID, nullptr, nullptr, // no username/password
                               nullptr,                     // no will
                               0, false, nullptr,
                               /* cleanSession: */ false))
        {
            break;
        }

        // if (mqttClient.connect(CLIENT_ID))
        // {
        //     break;
        // }

        Log.errorln("Failed to connect to IoT Core.");
        // secureClient->stop();
        coreDelay(500);
    }
    MQTTConnected = mqttClient.connected();
    Log.noticeln("MQTT Connected: %d %s", i, String(MQTTConnected).c_str());
    if (i > MAX_CONNECTION_ATTEMPTS && !MQTTConnected)
    {
        // connectCount >= 1
        if (connectCount >= 1)
        {
            Log.errorln("Restoring IoT connection.");
            connection.restore();
        }

        return false;
    }
    Log.noticeln("Verifing Connection to Secure MQTT Network.");
    // the LED pin will show that the connection is established
    // MQTTConnected = mqttClient.connected();

    if (MQTTConnected)
    {
        subscribeToTopics();
        Log.noticeln("Connected to IoT Core.");
        light.startBreathing();
        connectCount = 0;
    }
    Log.noticeln("MQTT initialized");
    initialized = true;
    return MQTTConnected;
}
/**
 * @brief attempts to reconnect to the MQTT server and the network connection
 */
bool SecureMQTTProcessor::reconnect()
{
    Log.noticeln("Reconnecting to IoT Core...");
    disconnect();
    coreDelay(500);
    return init();
}

/**
 * @brief Disconnects the MQTT client and the network connection
 */
void SecureMQTTProcessor::disconnect()
{
    MQTTConnected = false;
    light.endBreathing();
    if (mqttClient.connected())
    {
        mqttClient.disconnect();
    }
    coreDelay(300);
    restartSSL();
    coreDelay(300);
    Log.noticeln("Disconnecting from MQTT server...");
}
/**
 * @brief checks if the MQTT client is connected
 * @return true - if the MQTT client is connected
 */
bool SecureMQTTProcessor::isConnected()
{
    return mqttClient.connected();
}

/**
 * @brief push a message to the MQTT server using a topic and a payload
 *
 * @param const char * topic
 * @param const char * payload
 * @return true - if successful
 */
bool SecureMQTTProcessor::publish(const char *topic, const char *payload)
{
    if (!mqttClient.connected())
    {
        Log.errorln("MQTT client is not connected.");
        return false;
    }
    return mqttClient.publish(topic, payload);
}

/**
 * @brief subscribes to a topic on the MQTT server
 *
 * @param const char *topic topic
 * @return true - if successful
 */
bool SecureMQTTProcessor::subscribeToTopic(const char *topic)
{
    Log.notice(F("SUBSCRIBING TO: %s" CR), topic);
    return mqttClient.subscribe(topic);
}

/**
 * @brief subscribes to a topic on the MQTT server
 *
 * @param const char *topic topic
 * @return true - if successful
 */
bool SecureMQTTProcessor::unsubscribeToTopic(const char *topic)
{
    Log.notice(F("UNSUBSCRIBING TO: %s" CR), topic);
    return mqttClient.unsubscribe(topic);
}

/**
 * @brief on disconnect, we find that the topics will need to be re-subscribed to, this method
 * will iterate through previously subscribed topics and re-subscribe to them.
 */
void SecureMQTTProcessor::subscribeToTopics()
{
    CommunicationRegistry::getInstance().iterateCallbacks([this](const char *topic)
                                                          { subscribeToTopic(topic); });
}

/**
 * @brief subscribes to a topic and registers a callback function that will be called when the topic is published to.
 *
 * @param const char * topic
 * @param std::function<void(const char *, const char *)> callback callback
 * @return true - if successful
 */
bool SecureMQTTProcessor::subscribe(const char *topic, std::function<void(const char *, const char *)> callback)
{
    if (!CommunicationRegistry::getInstance().hasCallback(topic))
    {
        subscribeToTopic(topic);
    }
    CommunicationRegistry::getInstance().registerCallback(topic, callback);
    return true;
}

bool SecureMQTTProcessor::unsubscribe(const char *topic)
{
    if (CommunicationRegistry::getInstance().hasCallback(topic))
    {
        CommunicationRegistry::getInstance().unregisterCallback(topic);
        return unsubscribeToTopic(topic);
    }
    return false;
}

/**
 * @brief all subscribed topics are returned to this callback. It manages what functions are called
 *
 * @param char *topic
 * @param byte *payload
 * @param unsigned int  length
 */
void SecureMQTTProcessor::mqttCallback(char *topic, byte *payload, unsigned int length)
{
    String message = "";
    for (unsigned int i = 0; i < length; i++)
    {
        char c = (char)payload[i];
        message += c;
    }
    CommunicationRegistry::getInstance().triggerCallbacks(topic, message.c_str());
}