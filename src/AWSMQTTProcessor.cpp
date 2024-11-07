#include "AWSMQTTProcessor.h"

AWSMQTTProcessor::AWSMQTTProcessor(Connection &connection)
    : connection(connection),
      connectionClient(connection.getClient()),
      sslClient(connectionClient)
{
    mqttClient.setClient(sslClient);
}

bool AWSMQTTProcessor::init()
{
    while (!connection.init())
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
    // mqttClient.flush();
    mqttClient.setKeepAlive(30);
    mqttClient.setServer(AWS_IOT_ENDPOINT, AWS_IOT_PORT);
    mqttClient.setCallback([this](char *topic, byte *payload, unsigned int length)
                           { mqttCallback(topic, payload, length); });
    Serial.println("MQTT initialized");
    subscribeToTopics();
    return connect();
}

bool AWSMQTTProcessor::keepAliveReady()
{
    if (millis() - lastInActivity > KEEP_ALIVE_INTERVAL)
    {
        lastInActivity = millis();
        return true;
    }
    return false;
}

// void AWSMQTTProcessor::loopMaintain()
// {
//     digitalWrite(LED_PIN, !digitalRead(LED_PIN));
//     mqttClient.loop();
// }

void AWSMQTTProcessor::loop()
{
    lastInActivity = millis();
    bool connected = mqttClient.connected();
    if (connected)
    {
        return;
    }

    // if (THREAD_MODE == 0 && connected && false)
    // {
    //     return loopMaintain();
    // }
    // else if (connected)
    // {
    //     return;
    // }

    if (!connection.isConnected())
    {
        connection.connect();
    }
    else
    {
        Serial.println("Reconnecting to MQTT...");
        reconnect();
    }
}

// void AWSMQTTProcessor::setThread()
// {
//     if (maintainConnectHandle != NULL)
//     {
//         return;
//     }
//     // can't find the sweet spot for size to run this in thread
//     xTaskCreatePinnedToCore(maintainConnection, "MaintainConnection", 8192, this, 1, &maintainConnectHandle, 1);
// }

void AWSMQTTProcessor::maintenanceCallback(AWSMQTTProcessor *instance)
{
    static_cast<AWSMQTTProcessor *>(instance)->runMaintenanceConnection();
}

void AWSMQTTProcessor::startMaintenanceLoop()
{
    tick.attach_ms<AWSMQTTProcessor *>(500, &AWSMQTTProcessor::maintenanceCallback, this);
}

void AWSMQTTProcessor::runMaintenanceConnection()
{
    if (!isConnected())
    {
        Serial.println("Breaking Loop Thread...");
        digitalWrite(LED_PIN, LOW);
        return tick.detach();
    }
    vTaskDelay(1); // Delay for 1 tick (typically 1 ms)
    if (keepAliveReady())
    {
        loop();
    }
    vTaskDelay(1); // Delay for 1 tick (typically 1 ms)
    mqttClient.loop();
    vTaskDelay(1); // Delay for 1 tick (typically 1 ms)
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    // vTaskDelay(1); // Delay for 1 tick (typically 1 ms)
}
// /**
//  * @brief Seems to crash the system when running in thread mode
//  *
//  * @param param
//  */
// void AWSMQTTProcessor::maintainConnection(void *param)
// {
//     AWSMQTTProcessor *processor = static_cast<AWSMQTTProcessor *>(param);
//     while (1)
//     {
//         digitalWrite(LED_PIN, LOW);
//         if (!processor->isConnected())
//         {
//             Serial.println("Breaking Loop Thread...");
//             break;
//         }
//         processor->loopMaintain();
//         taskYIELD();
//         digitalWrite(LED_PIN, HIGH);
//         vTaskDelay(50);
//         digitalWrite(LED_PIN, LOW);
//         vTaskDelay(1000);
//     }
// }

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
bool AWSMQTTProcessor::hardDisconnect()
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
        Serial.println("Failed to turn on cellular network.");
        return false;
    }
    return init();
}
bool AWSMQTTProcessor::connect()
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
            Serial.println("Failed to connect cellular network.");
            return false;
        }
    }
    connectCount++;
    return setupAWSConnection();
}

bool AWSMQTTProcessor::setupAWSConnection()
{
    // destroyThread();
    Serial.print("Setting up AWS connection... ");
    Serial.println(AWS_CLIENT_ID);

    if (!mqttClient.connect(AWS_CLIENT_ID))
    {
        Serial.println("Failed to connect to AWS IoT Core.");
        return false;
    }

    Serial.println("Connected to AWS IoT Core.");

    // if (THREAD_MODE == 1)
    // {
    //     setThread();
    // }
    startMaintenanceLoop();
    // subscribeToTopics();
    connectCount = 0;
    return true;
}

// void AWSMQTTProcessor::destroyThread()
// {
//     if (maintainConnectHandle == NULL)
//     {
//         return;
//     }
//     vTaskDelete(maintainConnectHandle);
//     maintainConnectHandle = NULL;
//     delay(300);
// }

void AWSMQTTProcessor::reconnect()
{
    Serial.println("Reconnecting to AWS IoT Core...");
    disconnect();
    delay(500);
    connectServer();
}

void AWSMQTTProcessor::disconnect()
{
    digitalWrite(LED_PIN, LOW);
    tick.detach();
    delay(100);
    // destroyThread();
    mqttClient.disconnect();
    sslClient.stop();
    delay(300);
}

bool AWSMQTTProcessor::isConnected()
{
    return mqttClient.connected();
}

bool AWSMQTTProcessor::publish(const char *topic, const char *payload)
{
    return mqttClient.publish(topic, payload);
}

bool AWSMQTTProcessor::subscribeToTopic(const char *topic)
{
    Serial.print("SUBSCRIBING TO: ");
    Serial.println(topic);
    return mqttClient.subscribe(topic);
}

void AWSMQTTProcessor::subscribeToTopics()
{
    CommunicationRegistry::getInstance().iterateCallbacks([this](const char *topic)
                                                          { subscribeToTopic(topic); });
}

bool AWSMQTTProcessor::subscribe(const char *topic, std::function<void(const char *, const char *)> callback)
{
    if (!CommunicationRegistry::getInstance().hasCallback(topic))
    {
        subscribeToTopic(topic);
    }
    CommunicationRegistry::getInstance().registerCallback(topic, callback);
    return true;
}

void AWSMQTTProcessor::mqttCallback(char *topic, byte *payload, unsigned int length)
{
    String message = "";
    for (unsigned int i = 0; i < length; i++)
    {
        char c = (char)payload[i];
        message += c;
    }
    CommunicationRegistry::getInstance().triggerCallbacks(topic, message.c_str());
}