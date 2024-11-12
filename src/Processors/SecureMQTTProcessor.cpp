#include "processors/SecureMQTTProcessor.h"

SecureMQTTProcessor::SecureMQTTProcessor(Connection &connection)
    : connection(connection)
{
}

bool SecureMQTTProcessor::init()
{
    while (!connection.init())
        ;
    lastInActivity = millis();
    Log.noticeln("Network Connection Initialized");
    return connectServer();
}

bool SecureMQTTProcessor::connectServer()
{
    if (!connection.isConnected())
    {
        Log.errorln("Network connection is not established.");
        return false;
    }
    Client *netClient = connection.getClient();
    if (netClient == nullptr)
    {
        Log.errorln("Failed to get network client.");
        return false;
    }
    // slight delay to allow the modem to get ready for a new connection
    delay(1000);

    if (!loadCertificates())
    {
        Log.errorln("Failed to load certificates.");
        return false;
    }

    // delay(300);
    sslClient.setClient(netClient);
    mqttClient.setClient(sslClient);
    mqttClient.setKeepAlive(KEEP_ALIVE);
    mqttClient.setServer(MQTT_IOT_ENDPOINT, MQTT_IOT_PORT);
    mqttClient.setCallback([this](char *topic, byte *payload, unsigned int length)
                           { mqttCallback(topic, payload, length); });
    Log.noticeln("MQTT initialized");
    return connect();
}

bool SecureMQTTProcessor::keepAliveReady()
{
    if (millis() - lastInActivity > KEEP_ALIVE_INTERVAL)
    {
        lastInActivity = millis();
        return true;
    }
    return false;
}

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

bool SecureMQTTProcessor::loadCertificates()
{
    if (!SPIFFS.begin(true))
    {
        Log.errorln("Failed to initialize SPIFFS");
        return false;
    }
    // Load Amazon Root CA
    File ca = SPIFFS.open("/aws-root-ca.pem", "r");
    if (!ca)
    {
        Log.errorln("Failed to open CA file");
        return false;
    }
    String caContent = ca.readString();
    ca.close();
    if (caContent.isEmpty())
    {
        Log.errorln("CA file is empty");
        return false;
    }
    const char *caContentC = caContent.c_str();
    Log.noticeln("Loaded CA certificate:");
    Log.noticeln(caContentC);
    sslClient.setCACert(caContentC);
    // Load Device Certificate
    File cert = SPIFFS.open("/aws-device-cert.pem", "r");
    if (!cert)
    {
        Log.errorln("Failed to open device certificate file");
        return false;
    }
    String certContent = cert.readString();
    cert.close();
    if (certContent.isEmpty())
    {
        Log.errorln("Device certificate file is empty");
        return false;
    }
    const char *certContentC = certContent.c_str();
    Log.noticeln("Loaded device certificate:");
    Log.noticeln(certContentC);
    sslClient.setCertificate(certContentC);
    // Load Device Private Key
    File key = SPIFFS.open("/aws-private-key.pem", "r");
    if (!key)
    {
        Log.errorln("Failed to open private key file");
        return false;
    }
    String keyContent = key.readString();
    key.close();
    if (keyContent.isEmpty())
    {
        Log.errorln("Private key file is empty");
        return false;
    }
    const char *keyContentC = keyContent.c_str();
    Log.noticeln("Loaded private key:");
    Log.noticeln(keyContentC);
    sslClient.setPrivateKey(keyContentC);
    SPIFFS.end();
    return true;
}
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
    return setupAWSConnection();
}

bool SecureMQTTProcessor::setupAWSConnection()
{
    Log.notice("Setting up AWS connection... ");
    Log.noticeln(AWS_CLIENT_ID);

    if (!mqttClient.connect(AWS_CLIENT_ID))
    {
        Log.errorln("Failed to connect to AWS IoT Core.");
        return false;
    }

    subscribeToTopics();
    Log.noticeln("Connected to AWS IoT Core.");
    light.startBreathing();
    connectCount = 0;
    return true;
}

void SecureMQTTProcessor::reconnect()
{
    Log.noticeln("Reconnecting to AWS IoT Core...");
    disconnect();
    delay(500);
    connectServer();
}

void SecureMQTTProcessor::disconnect()
{
    digitalWrite(LED_PIN, LOW);
    light.endBreathing();
    delay(100);
    mqttClient.disconnect();
    sslClient.stop();
    delay(300);
}

bool SecureMQTTProcessor::isConnected()
{
    return mqttClient.connected();
}

bool SecureMQTTProcessor::publish(const char *topic, const char *payload)
{
    return mqttClient.publish(topic, payload);
}

bool SecureMQTTProcessor::subscribeToTopic(const char *topic)
{
    Log.notice(F("SUBSCRIBING TO: %s"), topic);
    return mqttClient.subscribe(topic);
}

void SecureMQTTProcessor::subscribeToTopics()
{
    CommunicationRegistry::getInstance().iterateCallbacks([this](const char *topic)
                                                          { subscribeToTopic(topic); });
}

bool SecureMQTTProcessor::subscribe(const char *topic, std::function<void(const char *, const char *)> callback)
{
    if (!CommunicationRegistry::getInstance().hasCallback(topic))
    {
        subscribeToTopic(topic);
    }
    CommunicationRegistry::getInstance().registerCallback(topic, callback);
    return true;
}

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