
#include "HyphenConnect.h"
Ticker tick;
HyphenConnect hyphen(ConnectionType::WIFI_PREFERRED);
bool payloadReady = false;
long ticker = 0;

String sendMessage()
{
    JsonDocument doc;
    doc["message"] = "Hello from HyphenConnect";
    doc["id"] = DEVICE_PUBLIC_ID;
    String send;
    serializeJson(doc, send);
    return send;
}

void sendPayload()
{
    payloadReady = false;
    if (!hyphen.isConnected())
    {
        Serial.println("Not connected to cellular network");
        return;
    }

    if (!hyphen.publishTopic("Hy/Post/Message", sendMessage()))
    {
        Serial.println("Failed to publish message");
        return;
    }
    Serial.println("Published message");
}

int sendTickerValuePlusOne(const char *params)
{
    Serial.printf("Calling all functions %s\n", params);
    return 1 + ticker;
}

void setup()
{

    while (!hyphen.setup())
        ;
    hyphen.subscribe("Hy/Post/Config", [](const char *topic, const char *payload)
                     {
    Serial.printf("Received message on topic: %s\n", topic);
    Serial.printf("Payload: %s\n", payload); });
    // register function
    hyphen.function("sendTickerValuePlusOne", sendTickerValuePlusOne);
    hyphen.variable("tickerValue", &ticker);
    // our job interval
    tick.attach(10, []()
                { payloadReady = true; });
}

void loop()
{
    hyphen.loop();
    if (payloadReady)
    {
        sendPayload();
        ticker++;
    }
}