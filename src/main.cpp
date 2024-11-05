

#include "Cellular.h"
#include "AWSMQTTProcessor.h"
Cellular cellular;
AWSMQTTProcessor mqttProcessor(&cellular);

void setup()
{
  Serial.begin(115200);

  mqttProcessor.init();
  // mqttProcessor.subscribe("AI/Post/Config");
}

void loop()
{
  mqttProcessor.loop();
  int quality = cellular.getSignalQuality();
  Serial.print("RSSI: ");
  Serial.println(quality);
  bool connected = mqttProcessor.isConnected();
  Serial.print("MQTT Connected: ");
  Serial.println(connected);

  if (connected)
  {
    if (!mqttProcessor.publish("AI/Post/Boomo", "{\"Boomo\":\"I love Alina\"}"))
    {
      Serial.println("Failed to publish message");
    }
  }

  delay(10000); // Adjust as necessary
}