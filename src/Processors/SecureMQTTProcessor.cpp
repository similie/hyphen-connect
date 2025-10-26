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
    initialized = false;
    health.on();
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
    ;
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

bool SecureMQTTProcessor::cleanupDisconnect()
{
    Log.errorln("Connection is not established.");
    stop();
    return false;
}

bool SecureMQTTProcessor::runMaintenance()
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
    if (MQTTConnected)
    {
        return true;
    }
    Log.noticeln("Reconnecting to MQTT...");
    return reconnect();
}

void SecureMQTTProcessor::setMaintenanceTicker()
{
    if (_maintenanceTimer != nullptr)
    {
        return resetMaintenanceTicker();
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

void SecureMQTTProcessor::resetMaintenanceTicker()
{
    stopMaintenanceTicker();
    setMaintenanceTicker();
}

void SecureMQTTProcessor::stopMaintenanceTicker()
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

void SecureMQTTProcessor::threadConnectionMaintenance(void *pv)
{
    static_cast<SecureMQTTProcessor *>(pv)->maintain();
    vTaskDelete(NULL);
}

void SecureMQTTProcessor::threadConnectionMaintenance()
{
    xTaskCreatePinnedToCore(&SecureMQTTProcessor::threadConnectionMaintenance,
                            "HyphenMaintenance",
                            4096 / sizeof(StackType_t),
                            // allocatedStack / sizeof(StackType_t),
                            this,
                            tskIDLE_PRIORITY + 1,
                            &maintenceHandle,
                            1);
}

void SecureMQTTProcessor::maintenanceCallback(SecureMQTTProcessor *instance)
{
    SecureMQTTProcessor *processor = static_cast<SecureMQTTProcessor *>(instance);
    processor->toggleMaintenance();
}

void SecureMQTTProcessor::toggleMaintenance()
{
    Log.noticeln("Toggle maintenance...");
    health.interrogate(); // check if the health is ok. We have this process even if things seem locked up
    isMaintenanceRunning = true;
}

bool SecureMQTTProcessor::maintain()
{
    // if we are not connected, we need to reconnect, we will check every n seconds
    if (!keepAliveReady())
    {
        return true;
    }
    maintenanceEvent = true;
    coreDelay(10);
    // if during this execution the loop is called, we will wait for it to finish
    Log.noticeln("Starting maintenance... %d %d", loopEvent, processing);
    Log.noticeln("Maintenance locked");
    bool mainained = runMaintenance();
    maintenanceEvent = false;
    return mainained;
}

bool SecureMQTTProcessor::loopNotReady()
{
    return !MQTTConnected;
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
    health.loop();
    mqttClient.loop();
    loopEvent = false;
}

bool SecureMQTTProcessor::attachCertificates()
{
#ifndef INSECURE_MQTT
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
    if (certsCached)
    {
        return attachCertificates();
    }

// ✅ Try to use embedded certificates first
#if defined(_binary_src_certs_root_ca_pem_start)
    String caContent((const char *)_binary_src_certs_root_ca_pem_start,
                     (size_t)(_binary_src_certs_root_ca_pem_end - _binary_src_certs_root_ca_pem_start));
    String certContent((const char *)_binary_src_certs_device_cert_pem_start,
                       (size_t)(_binary_src_certs_device_cert_pem_end - _binary_src_certs_device_cert_pem_start));
    String keyContent((const char *)_binary_src_certs_private_key_pem_start,
                      (size_t)(_binary_src_certs_private_key_pem_end - _binary_src_certs_private_key_pem_start));

    if (!caContent.isEmpty() && !certContent.isEmpty() && !keyContent.isEmpty())
    {
        certificates[CA] = caContent;
        certificates[DeviceCertificate] = certContent;
        certificates[DevicePrivateKey] = keyContent;
        certsCached = true;
        Log.noticeln("✅ Loaded embedded certificates from flash.");
        return attachCertificates();
    }
    else
    {
        Log.warningln("⚠️ Embedded certificates missing or empty, falling back to SPIFFS.");
    }
#endif

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

void SecureMQTTProcessor::restartSSL()
{
    Log.noticeln("Restarting SSL connection...");
#ifndef INSECURE_MQTT
    if (secureClient == nullptr)
    {
        Log.noticeln("Secure client is null");
        return;
    }
    secureClient = nullptr;
#else
    if (client == nullptr)
    {
        Log.noticeln("Client is null");
        return;
    }
    client = nullptr;
#endif
}

bool SecureMQTTProcessor::connect()
{
    connectCount++;
    health.loop();
    setMaintenanceTicker();
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
        coreDelay(500);
    }
    MQTTConnected = mqttClient.connected();
    Log.noticeln("MQTT Connected: %d %s", i, String(MQTTConnected).c_str());
    if (i > MAX_CONNECTION_ATTEMPTS && !MQTTConnected)
    {
        if (connectCount >= restorationAttempts)
        {
            Log.errorln("Restoring IoT connection.");
            connectCount = 0;
            health.reboot();
        }

        return false;
    }
    Log.noticeln("Verifing Connection to Secure MQTT Network.");
    if (MQTTConnected)
    {
        subscribeToTopics();
        Log.noticeln("Connected to IoT Core.");
        light.startBreathing();
        connectCount = 0;
    }
    Log.noticeln("MQTT initialized");
    initialized = true;
    setMaintenanceTicker();
    return MQTTConnected;
}
/**
 * @brief attempts to reconnect to the MQTT server and the network connection
 */
bool SecureMQTTProcessor::reconnect()
{
    Log.noticeln("Reconnecting to IoT Core...");
    stop();
    coreDelay(500);
    return init();
}

void SecureMQTTProcessor::stop()
{
    Log.noticeln("Disconnecting from MQTT server...");
    MQTTConnected = false;
    light.endBreathing();
    mqttClient.disconnect();
}

/**
 * @brief Disconnects the MQTT client and the network connection
 */
void SecureMQTTProcessor::disconnect()
{
    health.off();
    stopMaintenanceTicker();
    stop();
}
/**
 * @brief checks if the MQTT client is connected
 * @return true - if the MQTT client is connected
 */
bool SecureMQTTProcessor::isConnected()
{
    if (!MQTTConnected)
    {
        return false;
    }
    MQTTConnected = mqttClient.connected();
    return MQTTConnected;
}

bool SecureMQTTProcessor::onDisconnect()
{
    stop();
    return false;
}

bool SecureMQTTProcessor::waitForMaintenance()
{
    unsigned long startMillis = millis();
    const unsigned long timeout = 10000; // 10 seconds timeout
    while (maintenanceEvent || loopEvent)
    {
        coreDelay(10);
        if (millis() - startMillis > timeout)
        {
            Log.errorln("Maintenance wait timed out.");
            return false; // Timeout reached
        }
    }
    return true; // Maintenance completed
}

bool SecureMQTTProcessor::preProcesss()
{
    processing = true;
    if (!isConnected())
    {
        processing = false;
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
        CommunicationRegistry::getInstance().registerCallback(topic, callback);
    }
    // CommunicationRegistry::getInstance().registerCallback(topic, callback);
    return subscribeToTopic(topic);
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
    Serial.println("MQTT Callback: " + String(topic) + " " + message);
    CommunicationRegistry::getInstance().triggerCallbacks(topic, message.c_str());
}