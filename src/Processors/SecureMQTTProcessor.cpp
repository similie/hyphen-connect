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
    Lock l;
    initialized = false;
    if (!connection.isConnected())
    {
        return false;
    }
    // wait a tick for the network to be ready
    return connectServer();
}

bool SecureMQTTProcessor::ready()
{
    return initialized && isConnected();
}

bool SecureMQTTProcessor::attachClients()
{
#ifndef INSECURE_MQTT
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
#else
    client = &connection.getClient();
    if (client == nullptr)
    {
        Log.errorln("Secure client is set to null.");
        return false;
    }
#endif
    return true;
}

bool SecureMQTTProcessor::attachServer()

{
    // Lock l;
#ifndef INSECURE_MQTT
    if (secureClient == nullptr)
    {
        Log.errorln("Secure client is set to null.");
        return false;
    }
#else
    if (client == nullptr)
    {
        Log.errorln("Client is set to null.");
        return false;
    }
#endif
    // sslClient.validate(MQTT_IOT_ENDPOINT, MQTT_IOT_PORT);
    mqttClient
        .setServer(MQTT_IOT_ENDPOINT, MQTT_IOT_PORT)
#ifndef INSECURE_MQTT
        .setClient(*secureClient)
#else
        .setClient(*client)
#endif
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
    return isMaintenanceRunning;
}

void SecureMQTTProcessor::cleanupDisconnect()
{
    Log.errorln("Connection is not established.");
    if (MQTTConnected)
    {
        stop();
    }
    connection.maintain();
}

void SecureMQTTProcessor::runMaintenance()
{
    Log.noticeln("Running maintenance... %s %s", String(MQTTConnected), String(ready()));
    isMaintenanceRunning = false;
    // maintain the connection
    if (!connection.isConnected())
    {
        return cleanupDisconnect();
    }
    MQTTConnected = mqttClient.connected();
    Log.noticeln("MQTT CONNECTION STATUS %s", String(MQTTConnected));
    connection.maintain();
    if (MQTTConnected)
    {
        return;
    }
    Log.noticeln("Reconnecting to MQTT...");
    reconnect();
}

void SecureMQTTProcessor::setMaintenaceTicker()
{
    if (_maintenanceTimer != nullptr)
    {
        return resetMaintenaceTicker();
    }
    Log.noticeln("Setting up maintenance ticker...");
    isMaintenanceRunning = false;
    esp_timer_create_args_t args = {
        .callback = [](void *ctx)
        {
            static_cast<SecureMQTTProcessor *>(ctx)->toggleMaintenance();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "maint-timer"};
    esp_timer_create(&args, &_maintenanceTimer);
    esp_timer_start_periodic(_maintenanceTimer, KEEP_ALIVE_INTERVAL * 1000UL);
}

void SecureMQTTProcessor::resetMaintenaceTicker()
{
    stopMaintenaceTicker();
    setMaintenaceTicker();
}

void SecureMQTTProcessor::stopMaintenaceTicker()
{
    if (_maintenanceTimer == nullptr)
    {
        return;
    }
    esp_timer_stop(_maintenanceTimer);
    esp_timer_delete(_maintenanceTimer);
    _maintenanceTimer = nullptr;
    isMaintenanceRunning = false;
}

void SecureMQTTProcessor::toggleMaintenance()
{
    Log.noticeln("Toggle maintenance...");
    isMaintenanceRunning = true;
}

void SecureMQTTProcessor::maintain()
{
    if (processing || loopEvent)
    {
        return;
    }

    // loop();
    // if we are not connected, we need to reconnect, we will check every n seconds
    if (!keepAliveReady())
    {
        return;
    }
    maintenceEvent = true;
    // if during this execution the loop is called, we will wait for it to finish
    while (loopEvent || processing)
        ;
    Lock l;
    runMaintenance();
    maintenceEvent = false;
}

bool SecureMQTTProcessor::waitForMaintenance()
{
    processing = true;
    unsigned long startMillis = millis();
    const unsigned long timeout = 5000; // 5 seconds timeout
    while (maintenceEvent || loopEvent)
    {
        coreDelay(10);
        if (millis() - startMillis > timeout)
        {
            Log.errorln("Maintenance wait timed out.");
            processing = false;
            return false; // Timeout reached
        }
    }
    return true; // Maintenance completed
}

bool SecureMQTTProcessor::loopNotReady()
{
    return processing || maintenceEvent || !MQTTConnected;
}

/**
 * @brief runs on the loop and maintains the connection.
 * your main loop should not contain any blocking code
 */
void SecureMQTTProcessor::loop()
{
    if (loopNotReady())
    {
        return;
    }
    loopEvent = true;
    mqttClient.loop();
    loopEvent = false;
}

bool SecureMQTTProcessor::attachCertificates()
{
    // Lock l;
#ifndef INSECURE_MQTT
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
#endif
    return true;
}

/**
 * @brief loads the certificates from the secure MQTT connection
 *
 * @return true - all certificates are loaded
 */
bool SecureMQTTProcessor::loadCertificates()
{
    // Lock l;
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

bool SecureMQTTProcessor::connect()
{
    Lock l;
    connectCount++;
    setMaintenaceTicker();
    return setupSecureConnection();
}

/**
 * @brief connects to the MQTT server using the client id
 * @return true - if the connection is successful
 */
bool SecureMQTTProcessor::setupSecureConnection()
{
    // Lock l;
    Log.notice("Setting up IoT connection... ");
    Log.noticeln(CLIENT_ID);
    uint8_t i = 0;
    for (i; i <= MAX_CONNECTION_ATTEMPTS; i++)
    {
        if (!connection.isConnected())
        {
            return false;
        }

#ifndef INSECURE_MQTT
        if (mqttClient.connect(CLIENT_ID, nullptr, nullptr, // no username/password
                               nullptr,                     // no will
                               0, false, nullptr,
                               /* cleanSession: */ false))
        {
            break;
        }
#else
        if (mqttClient.connect(CLIENT_ID, String(MQTT_USERNAME).c_str(), String(MQTT_PASSWORD).c_str(), // no username/password
                               nullptr,                                                                 // no will
                               0, false, nullptr,
                               /* cleanSession: */ false))
        {
            break;
        }
#endif

        Log.errorln("Failed to connect to IoT Core.");
        // secureClient->stop();
        coreDelay(500);
    }
    MQTTConnected = mqttClient.connected();
    Log.noticeln("MQTT Connected: %d %s", i, String(MQTTConnected).c_str());
    if (i > MAX_CONNECTION_ATTEMPTS && !MQTTConnected)
    {
        // connectCount >= 1
        if (connectCount >= restorationAttempts)
        {
            Log.errorln("Restoring IoT connection.");
            connectCount = 0;
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
    setMaintenaceTicker();
    return MQTTConnected;
}
/**
 * @brief attempts to reconnect to the MQTT server and the network connection
 */
bool SecureMQTTProcessor::reconnect()
{
    // Lock l;
    Log.noticeln("Reconnecting to IoT Core...");
    stop();
    coreDelay(500);
    return init();
}

void SecureMQTTProcessor::stop()
{
    // Lock l;
    Log.noticeln("Disconnecting from MQTT server...");
    MQTTConnected = false;
    // initialized = false;
    light.endBreathing();
    mqttClient.disconnect();
    // restartSSL();
}

/**
 * @brief Disconnects the MQTT client and the network connection
 */
void SecureMQTTProcessor::disconnect()
{
    Lock l;
    stopMaintenaceTicker();
    stop();
}
/**
 * @brief checks if the MQTT client is connected
 * @return true - if the MQTT client is connected
 */
bool SecureMQTTProcessor::isConnected()
{
    Lock l;
    if (!MQTTConnected)
    {
        return false;
    }
    return mqttClient.connected();
}

bool SecureMQTTProcessor::onDisconnect()
{
    if (!MQTTConnected)
    {
        return false;
    }

    Lock l;
    stop();
    return false;
}

bool SecureMQTTProcessor::preProcesss()
{
    if (!waitForMaintenance())
    {
        return false;
    }
    Lock l;
    if (!isConnected())
    {
        return onDisconnect();
    }
    return true;
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
    if (!preProcesss())
    {
        return false;
    }
    Lock l;
    bool published = mqttClient.publish(topic, payload);
    processing = false;
    return published;
}

bool SecureMQTTProcessor::publish(const char *topic, uint8_t *buf, size_t length)
{
    if (!preProcesss())
    {
        return false;
    }
    Lock l;
    processing = true;
    bool published = mqttClient.publish(topic, buf, length);
    processing = false;
    return published;
}

/**
 * @brief subscribes to a topic on the MQTT server
 *
 * @param const char *topic topic
 * @return true - if successful
 */
bool SecureMQTTProcessor::subscribeToTopic(const char *topic)
{
    Lock l;
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
    Lock l;
    Log.notice(F("UNSUBSCRIBING TO: %s" CR), topic);
    return mqttClient.unsubscribe(topic);
}

/**
 * @brief on disconnect, we find that the topics will need to be re-subscribed to, this method
 * will iterate through previously subscribed topics and re-subscribe to them.
 */
void SecureMQTTProcessor::subscribeToTopics()
{
    Lock l;
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
    Lock l;
    if (!CommunicationRegistry::getInstance().hasCallback(topic))
    {
        subscribeToTopic(topic);
    }
    CommunicationRegistry::getInstance().registerCallback(topic, callback);
    return true;
}

bool SecureMQTTProcessor::unsubscribe(const char *topic)
{
    Lock l;
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
    Lock l;
    String message = "";
    for (unsigned int i = 0; i < length; i++)
    {
        char c = (char)payload[i];
        message += c;
    }
    Serial.println("MQTT Callback: " + String(topic) + " " + message);
    CommunicationRegistry::getInstance().triggerCallbacks(topic, message.c_str());
}