#include "AWSMQTTProcessor.h"

AWSMQTTProcessor::AWSMQTTProcessor(Connection *connection)
    : connection(connection)
{
}

bool AWSMQTTProcessor::init()
{
    while (!connection->init())
        ;
    lastInActivity = millis();
    Serial.println("Cellular initialized");
    return connectServer();
}

bool AWSMQTTProcessor::connectServer()
{
    if (!loadCertificates())
    {
        Serial.println("Failed to load certificates.");
        return false;
    }
    mqttClient.setClient(connection->getClient());
    mqttClient.setServer(AWS_IOT_ENDPOINT, AWS_IOT_PORT);
    mqttClient.setCallback([this](char *topic, byte *payload, unsigned int length)
                           { mqttCallback(topic, payload, length); });
    Serial.println("MQTT initialized");
    return connect();
}

// IPAddress AWSMQTTProcessor::resolveHostname(const char *hostname)
// {
//     IPAddress resolvedIP;
//     // if (!cellular->getModem().getHostByName(hostname, resolvedIP)) {
//     //     Serial.print("DNS resolution failed for hostname: ");
//     //     Serial.println(hostname);
//     //     return IPAddress(); // Return an empty IP address if resolution fails
//     // }
//     Serial.print("Resolved IP for ");
//     Serial.print(hostname);
//     Serial.print(" is ");
//     Serial.println(resolvedIP);
//     return resolvedIP;
// }

bool AWSMQTTProcessor::keepAliveReady()
{
    if (millis() - lastInActivity > KEEP_ALIVE_INTERVAL)
    {
        lastInActivity = millis();
        return true;
    }
    return false;
}

void AWSMQTTProcessor::loopMaintain()
{
    if (mqttClient.connected())
    {
        mqttClient.loop();
    }
    else
    {
        if (!connection->isConnected())
        {
            connection->connect();
            lastInActivity = millis();
        }
        else
        {
            Serial.println("Reconnecting to MQTT...");
            connectServer();
        }

        return;
    }

    // if (keepAliveReady())
    // {
    //     Serial.println("KEEPING MYSELF ALIVE");
    //     connection->maintain();
    // }
}

void AWSMQTTProcessor::loop()
{
    if (THREAD_MODE == 0)
    {
        loopMaintain();
    }
}

void AWSMQTTProcessor::setThread()
{
    if (maintainConnectHandle != NULL)
    {
        return;
    }
    xTaskCreatePinnedToCore(maintainConnection, "MaintainConnection", 4096, this, 1, &maintainConnectHandle, 1);
    // xTaskCreatePinnedToCore(keepAliveTask, "KeepAliveTask", 4096, this, 1, &keepAliveHandle, 1);
}
void AWSMQTTProcessor::maintainConnection(void *param)
{
    AWSMQTTProcessor *processor = static_cast<AWSMQTTProcessor *>(param);
    while (1)
    {
        processor->loopMaintain();
        vTaskDelay(100);
    }
}

bool AWSMQTTProcessor::loadCertificates()
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

    connection->getClient().setCACert(caContent.c_str());

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
    connection->getClient().setCertificate(certContent.c_str());

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
    connection->getClient().setPrivateKey(keyContent.c_str());
    SPIFFS.end();
    return true;
}

bool AWSMQTTProcessor::connect()
{
    if (!connection->isConnected())
    {
        Serial.println("Waiting for cellular connection...");
        if (!connection->connect())
        {
            Serial.println("Failed to connect cellular network.");
            return false;
        }
    }

    return setupAWSConnection();
}

bool AWSMQTTProcessor::setupAWSConnection()
{
    Serial.print("Setting up AWS connection... ");
    Serial.println(AWS_CLIENT_ID);

    if (!mqttClient.connect(AWS_CLIENT_ID))
    {
        Serial.println("Failed to connect to AWS IoT Core.");
        return false;
    }

    Serial.println("Connected to AWS IoT Core.");

    if (THREAD_MODE == 1)
    {
        setThread();
    }

    return true;
}

void AWSMQTTProcessor::disconnect()
{

    if (maintainConnectHandle != NULL)
    {
        Serial.println("Deleting keepAlive task...");
        vTaskDelete(maintainConnectHandle);
        maintainConnectHandle = NULL;
    }
    delay(300);
    mqttClient.disconnect();
    connection->disconnect();
}

bool AWSMQTTProcessor::isConnected()
{
    return mqttClient.connected();
}

bool AWSMQTTProcessor::publish(const char *topic, const char *payload)
{
    return mqttClient.publish(topic, payload);
}

bool AWSMQTTProcessor::subscribe(const char *topic)
{
    return mqttClient.subscribe(topic);
}

void AWSMQTTProcessor::mqttCallback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived on topic: ");
    Serial.println(topic);
    Serial.print("Message: ");

    for (unsigned int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println();
}