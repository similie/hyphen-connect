
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
        Log.noticeln("Not connected to a network");
        return;
    }

    if (!hyphen.publishTopic("Hy/Post/Message", sendMessage()))
    {
        Log.noticeln("Failed to publish message");
        return;
    }
    Log.noticeln("Published message");
}

int sendTickerValuePlusOne(const char *params)
{
    Log.notice(F("Calling all functions %s" CR), params);
    return 1 + ticker;
}

void setup()
{

    while (!hyphen.setup(LOG_LEVEL_VERBOSE))
        ;
    hyphen.subscribe("Hy/Post/Config", [](const char *topic, const char *payload)
                     {
        Log.notice(F("Received message on topic: %s" CR), topic);
        Log.notice(F("Payload: %s" CR), payload); });
    // here we register a function
    hyphen.function("sendTickerValuePlusOne", sendTickerValuePlusOne);
    // and we register our tickerValue as a variable
    hyphen.variable("tickerValue", &ticker);
    // our job interval
    tick.attach(10, []()
                { payloadReady = true; });
}

void loop()
{
    hyphen.loop();
    if (!payloadReady)
    {
        return;
    }
    sendPayload();
    ticker++;
}