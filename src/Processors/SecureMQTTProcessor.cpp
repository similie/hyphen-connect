#include "processors/SecureMQTTProcessor.h"

SecureMQTTProcessor::SecureMQTTProcessor(Connection &connection)
    : connection(connection)
{
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
        if (!connection.init())
        {
            return false;
        }
    }
    // initialize the certificate storage
    loadCertificates();
    // wait a tick for the network to be ready
    delay(500);
    lastInActivity = millis();
    Log.noticeln("Network Connection Initialized");
    return connectServer();
}

bool SecureMQTTProcessor::attachClients()
{
    Client *netClient = connection.getClient();
    if (netClient == nullptr)
    {
        Log.errorln("Failed to get network client.");
        return false;
    }
    // slight delay to allow the modem to get ready for a new connection
    // may not be required in all use cases
    if (!loadCertificates())
    {
        Log.errorln("Failed to load certificates.");
        return false;
    }
    sslClient.setClient(netClient);
    // mqttClient.setClient(sslClient);
    return true;
}

bool SecureMQTTProcessor::attachServer()

{
    mqttClient
        .setServer(MQTT_IOT_ENDPOINT, MQTT_IOT_PORT)
        .setClient(sslClient)
        .setKeepAlive(KEEP_ALIVE)
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
    if (!connection.isConnected())
    {
        Log.errorln("Network connection is not established.");
        return false;
    }

    if (!attachClients())
    {
        return false;
    }

    if (!attachServer())
    {
        return false;
    }

    Log.noticeln("MQTT initialized");
    return connect();
}

/**
 * @brief returns true on an interval to keep the connection alive
 *
 * @return true - if the keep alive interval is ready
 */
bool SecureMQTTProcessor::keepAliveReady()
{
    if (millis() - lastInActivity > KEEP_ALIVE_INTERVAL)
    {
        lastInActivity = millis();
        return true;
    }
    return false;
}

/**
 * @brief runs on the loop and maintains the connection.
 * your main loop should not contain any blocking code
 */
void SecureMQTTProcessor::loop()
{
    mqttClient.loop();

    if (!keepAliveReady())
    {
        return;
    }
    connection.maintain();
    bool connected = mqttClient.connected();
    if (connected)
    {
        lastInActivity = millis();
        return;
    }
    light.endBreathing();

    if (!connection.isConnected())
    {
        connection.connect();
    }
    else
    {
        Log.noticeln("Reconnecting to MQTT...");
        reconnect();
    }
    lastInActivity = millis();
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
        switch (i)
        {
        case cachedCertificates::CA:
            sslClient.setCACert(certContentC);
            break;
        case cachedCertificates::DeviceCertificate:
            sslClient.setCertificate(certContentC);
            break;
        case cachedCertificates::DevicePrivateKey:
            sslClient.setPrivateKey(certContentC);
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
    delay(200);
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
    Log.noticeln(caContentC);
    delay(100);
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
    Log.noticeln(certContentC);
    delay(100);
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
    Log.noticeln(keyContentC);
    fm.end();
    certsCached = true;
    return true;
}

/**
 * @brief if the normal connection effort fails after a number of tries,
 * let's do a hard disconnect and restart the connection
 * @return true if successful, false otherwise
 */
bool SecureMQTTProcessor::hardDisconnect()
{
    Log.noticeln("Restarting connection with a hard disconnect...");
    connectCount = 0;
    disconnect();
    delay(100);
    connection.disconnect();
    connection.off();
    delay(1000);
    if (!connection.on())
    {
        Log.errorln("Failed to turn on network.");
        return false;
    }
    return init();
}
bool SecureMQTTProcessor::connect()
{
    if (connectCount >= HARD_CONNECTION_RESTART)
    {
        return hardDisconnect();
    }

    if (!connection.isConnected())
    {
        connectCount = 0;
        Log.noticeln("Waiting for cellular connection...");
        if (!connection.connect())
        {
            Log.errorln("Failed to connect network.");
            return false;
        }
    }
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

        if (mqttClient.connect(CLIENT_ID))
        {
            break;
        }
        Log.errorln("Failed to connect to IoT Core.");
        delay(500);
    }

    if (i >= MAX_CONNECTION_ATTEMPTS)
    {
        return false;
    }

    subscribeToTopics();
    Log.noticeln("Connected to IoT Core.");
    // the LED pin will show that the connection is established
    light.startBreathing();
    connectCount = 0;
    return true;
}
/**
 * @brief attempts to reconnect to the MQTT server and the network connection
 */
void SecureMQTTProcessor::reconnect()
{
    Log.noticeln("Reconnecting to IoT Core...");
    disconnect();
    delay(500);
    connectServer();
}
/**
 * @brief Disconnects the MQTT client and the network connection
 */
void SecureMQTTProcessor::disconnect()
{
    digitalWrite(LED_PIN, LOW);
    light.endBreathing();
    delay(100);
    mqttClient.disconnect();
    sslClient.stop();
    delay(300);
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