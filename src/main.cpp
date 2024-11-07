

#include "Cellular.h"
#include "AWSMQTTProcessor.h"
Cellular cellular;
AWSMQTTProcessor mqttProcessor(cellular);

bool payloadReady = false;

Ticker tick;

void sendPayload()
{
  payloadReady = false;
  if (!cellular.isConnected())
  {
    Serial.println("Not connected to cellular network");
    return;
  }

  int quality = cellular.getSignalQuality();
  Serial.print("RSSI: ");
  Serial.println(quality);
  bool connected = mqttProcessor.isConnected();
  Serial.print("MQTT Connected: ");
  Serial.println(connected);

  if (!connected)
  {
    Serial.println("Not connected to MQTT");
    return;
  }

  if (!mqttProcessor.publish("AI/Post/Boomo", "{\"Boomo\":\"I love Alina\"}"))
  {
    Serial.println("Failed to publish message");
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  // digitalWrite(IND_PIN, HIGH);

  mqttProcessor.init();
  mqttProcessor.subscribe("AI/Post/Config", [](const char *topic, const char *payload)
                          {
    Serial.printf("Received message on topic: %s\n", topic);
    Serial.printf("Payload: %s\n", payload); });

  tick.attach(10, []()
              { payloadReady = true; });
}

void loop()
{
  // mqttProcessor.loop();

  if (payloadReady)
  {
    sendPayload();
  }

  // delay(10000); // Adjust as necessary
}