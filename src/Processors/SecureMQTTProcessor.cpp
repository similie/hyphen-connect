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
    Serial.println("Cellular initialized");
    return connectServer();
}

bool SecureMQTTProcessor::connectServer()
{
    if (!connection.isConnected())
    {
        Serial.println("Network connection is not established.");
        return false;
    }
    Client *netClient = connection.getClient();
    if (netClient == nullptr)
    {
        Serial.println("Failed to get network client.");
        return false;
    }
    // slight delay to allow the modem to get ready for a new connection
    delay(1000);

    if (!loadCertificates())
    {
        Serial.println("Failed to load certificates.");
        return false;
    }

    // delay(300);
    sslClient.setClient(netClient);
    mqttClient.setClient(sslClient);
    mqttClient.setKeepAlive(KEEP_ALIVE);
    mqttClient.setServer(MQTT_IOT_ENDPOINT, MQTT_IOT_PORT);
    mqttClient.setCallback([this](char *topic, byte *payload, unsigned int length)
                           { mqttCallback(topic, payload, length); });
    Serial.println("MQTT initialized");
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
        Serial.println("Reconnecting to MQTT...");
        reconnect();
    }
    lastInActivity = millis();
}

bool SecureMQTTProcessor::loadCertificates()
{
    if (!SPIFFS.begin(true))
    {
        Serial.println("Failed to initialize SPIFFS");
        return false;
    }
    // Load Amazon Root CA
    File ca = SPIFFS.open("/aws-root-ca.pem", "r");
    if (!ca)
    {
        Serial.println("Failed to open CA file");
        return false;
    }
    String caContent = ca.readString();
    ca.close();
    if (caContent.isEmpty())
    {
        Serial.println("CA file is empty");
        return false;
    }
    Serial.println("Loaded CA certificate:");
    Serial.println(caContent);
    sslClient.setCACert(caContent.c_str());
    // Load Device Certificate
    File cert = SPIFFS.open("/aws-device-cert.pem", "r");
    if (!cert)
    {
        Serial.println("Failed to open device certificate file");
        return false;
    }
    String certContent = cert.readString();
    cert.close();
    if (certContent.isEmpty())
    {
        Serial.println("Device certificate file is empty");
        return false;
    }
    Serial.println("Loaded device certificate:");
    Serial.println(certContent);
    sslClient.setCertificate(certContent.c_str());
    // Load Device Private Key
    File key = SPIFFS.open("/aws-private-key.pem", "r");
    if (!key)
    {
        Serial.println("Failed to open private key file");
        return false;
    }
    String keyContent = key.readString();
    key.close();
    if (keyContent.isEmpty())
    {
        Serial.println("Private key file is empty");
        return false;
    }
    Serial.println("Loaded private key:");
    Serial.println(keyContent);
    sslClient.setPrivateKey(keyContent.c_str());
    SPIFFS.end();
    return true;
}
bool SecureMQTTProcessor::hardDisconnect()
{
    Serial.println("Restarting connection with a hard disconnect...");
    connectCount = 0;
    disconnect();
    delay(100);
    connection.disconnect();
    connection.off();
    delay(1000);
    if (!connection.on())
    {
        Serial.println("Failed to turn on network.");
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
        Serial.println("Waiting for cellular connection...");
        if (!connection.connect())
        {
            Serial.println("Failed to connect network.");
            return false;
        }
    }
    connectCount++;
    return setupAWSConnection();
}

bool SecureMQTTProcessor::setupAWSConnection()
{
    Serial.print("Setting up AWS connection... ");
    Serial.println(AWS_CLIENT_ID);

    if (!mqttClient.connect(AWS_CLIENT_ID))
    {
        Serial.println("Failed to connect to AWS IoT Core.");
        return false;
    }

    subscribeToTopics();
    Serial.println("Connected to AWS IoT Core.");

    light.startBreathing();
    connectCount = 0;
    return true;
}

void SecureMQTTProcessor::reconnect()
{
    Serial.println("Reconnecting to AWS IoT Core...");
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
    Serial.print("SUBSCRIBING TO: ");
    Serial.println(topic);
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