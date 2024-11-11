# HyphenConnect

HyphenConnect is an open-source library designed to simplify cloud connectivity for ESP32-based devices. It provides seamless integration with Wi-Fi and cellular networks, with plans to support LoRaWAN in the future.

## Features

    •	Dual Connectivity: Effortlessly switch between Wi-Fi and cellular networks to maintain robust connections.
    •	Cloud Integration: Simplifies publishing and subscribing to cloud-based topics.
    •	Extensibility: Easily register custom functions and variables for remote invocation and monitoring.
    •	Open Source: Licensed under the MIT License, encouraging community contributions and collaboration.

### Getting Started

#### Prerequisites

    •	Hardware: ESP32 development board.
    •	Software: Arduino IDE with ESP32 board support installed.

#### Installation

    1.	Clone the Repository:

git clone https://github.com/similie/HyphenConnect.git

    2.	Include the Library in Your Project:

Copy the HyphenConnect folder into your Arduino libraries directory.

#### Usage

Here’s an example of how to use HyphenConnect in your project:

```cpp
#include "HyphenConnect.h"
#include <Ticker.h>

Ticker tick;
HyphenConnect hyphen(ConnectionType::WIFI_PREFERRED);
bool payloadReady = false;
long tickerValue = 0;

String sendMessage() {
    StaticJsonDocument<200> doc;
    doc["message"] = "Hello from HyphenConnect";
    doc["id"] = DEVICE_PUBLIC_ID;
    String send;
    serializeJson(doc, send);
    return send;
}

void sendPayload() {
    payloadReady = false;
    if (!hyphen.isConnected()) {
        Serial.println("Not connected to network");
        return;
    }

    if (!hyphen.publishTopic("Hy/Post/Message", sendMessage())) {
        Serial.println("Failed to publish message");
        return;
    }
    Serial.println("Published message");
}

int sendTickerValuePlusOne(const char *params) {
    Serial.printf("Function called with params: %s\n", params);
    return 1 + tickerValue;
}

void setup() {
    Serial.begin(115200);
    while (!hyphen.setup()) {
        delay(1000);
    }
    hyphen.subscribe("Hy/Post/Config", [](const char *topic, const char *payload) {
        Serial.printf("Received message on topic: %s\n", topic);
        Serial.printf("Payload: %s\n", payload);
    });
    hyphen.function("sendTickerValuePlusOne", sendTickerValuePlusOne);
    hyphen.variable("tickerValue", &tickerValue);
    tick.attach(10, []() {
        payloadReady = true;
    });
}

void loop() {
    hyphen.loop();
    if (payloadReady) {
        sendPayload();
        tickerValue++;
    }
}
```

This example demonstrates how to:
• Initialize HyphenConnect with a preferred connection type.
• Publish messages to a specific topic.
• Subscribe to topics and handle incoming messages.
• Register custom functions and variables for remote access.

### MQTT Default Topics Commands

```javascript
/* The call ID is a unique identifier for each call. It is generated by HyphenConnect and sent to the device. It should be generated by the calling application and sent to the request.
 */
// This is used to call a function on the device.
"Hy/Post/Function/<DeviceId>/<FunctionName>/<CallId>"
// On startup, the device will send the functions that it supports. These can be stored on by your application. Note the devices sends these one at a time
"Hy/Post/Function/Register/<DeviceId>"
// This topic will receive the function results
"Hy/Post/Function/Result/<DeviceId>/<FunctionName>/<CallId>"
// The JSON results are as follows:
{
    "value": int,
    "key": "<FunctionName>",
    "id": "<DeviceId>",
    "request": "<CallId>" // this can be anything
}
// This is used to call a function on the device.
"Hy/Post/Variable/<DeviceId>/<FunctionName>/<CallId>"
// On startup, the device will send the variables that it supports. These can be stored on by your application. Note the devices sends these one at a time
"Hy/Post/Variable/Register/<DeviceId>"
// This topic will receive the variable value
"Hy/Post/Function/Variable/<DeviceId>/<FunctionName>/<CallId>"
// The JSON results are as follows:
{
    "value": any, // variable value
    "key": "<FunctionName>",
    "id": "<DeviceId>",
    "request": "<CallId>" // this can be anything
}
```

### About Similie

Similie is a small technology company based out of Timor-Leste, dedicated to developing innovative solutions that support international development initiatives. Our mission is to harness the power of technology to drive positive change and improve lives around the world. With a focus on sustainability, community engagement, and social impact, we strive to create products and services that make a real difference in people's lives.

### Contributions

Contributions are welcome! We are actively working on adding LoRaWAN support. If you’re interested in contributing, please fork the repository and submit a pull request.

### License

This project is licensed under the MIT License. See the LICENSE file for details.

### Acknowledgments

Special thanks to the open-source community for their invaluable contributions and support. Similie proudly produces open source technology that aims to benefit those impacted by the challenges of climate change.
