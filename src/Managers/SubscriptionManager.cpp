#include "managers/SubscriptionManager.h"

SubscriptionManager::SubscriptionManager(Processor &processor) : processor(processor)
{
}

bool SubscriptionManager::init()
{
    bool init = processor.init();
    if (!init)
    {
        return false;
    }

    processor.subscribe(functionTopic.c_str(), [this](const char *topic, const char *payload)
                        { functionalCallback(topic, payload); });

    processor.subscribe(variableTopic.c_str(), [this](const char *topic, const char *payload)
                        { variableCallback(topic, payload); });

    registerFunctions();
    return init;
}

bool SubscriptionManager::subscribe(const char *topic, std::function<void(const char *, const char *)> callback)
{
    return processor.subscribe(topic, callback);
}

void SubscriptionManager::registerFunctions()
{
    tick.attach(10, &SubscriptionManager::registrationCallback, this);
}

void SubscriptionManager::registrationCallback(SubscriptionManager *instance)
{
    SubscriptionManager *sub = static_cast<SubscriptionManager *>(instance);
    sub->setReady();
}

void SubscriptionManager::setReady()
{
    checkReady = true;
}

void SubscriptionManager::function(const char *topic, std::function<int(const char *)> callback)
{

    if (functionCount >= 20)
    {
        Serial.println("Function limit reached");
        return;
    }
    functionTopics[functionCount] = topic;
    functionCallbacks[topic] = callback;
    functionCount++;
}

String SubscriptionManager::getCallId(const char *topic)
{
    String topicStr = String(topic);
    int lastSlash = topicStr.lastIndexOf("/");
    return topicStr.substring(lastSlash + 1);
}

String SubscriptionManager::getTopicKey(const char *topic)
{
    String topicStr = String(topic);
    int lastSlash = topicStr.lastIndexOf("/");
    String keyBase = topicStr.substring(0, lastSlash);
    lastSlash = keyBase.lastIndexOf("/");
    return keyBase.substring(lastSlash + 1);
}

bool SubscriptionManager::isConnected()
{
    return processor.isConnected();
}

void SubscriptionManager::variableCallback(const char *topic, const char *payload)
{
    String callId = getCallId(topic);
    String key = getTopicKey(topic);
    auto varIt = variableRegistry.find(key.c_str());
    if (varIt == variableRegistry.end())
    {
        Serial.println("Variable or function not found.");
        return;
    }

    JsonDocument doc;
    doc["key"] = key;
    doc["id"] = deviceId;
    doc["request"] = callId;

    // Retrieve the variable value based on its type
    switch (varIt->second.type)
    {
    case VariableType::INT:
        doc["value"] = *(varIt->second.data.intPtr);
        break;
    case VariableType::LONG:
        doc["value"] = *(varIt->second.data.longPtr);
        break;
    case VariableType::STRING:
        doc["value"] = *(varIt->second.data.stringPtr);
        break;
    case VariableType::DOUBLE:
        doc["value"] = *(varIt->second.data.doublePtr);
        break;
    }

    // Serialize and publish the JSON document
    String resultStr;
    serializeJson(doc, resultStr);
    String sendTopic = variableResultsTopic + "/" + key + "/" + callId;
    if (!publishTopic(sendTopic, resultStr))
    {
        Serial.println("Failed to publish variable result");
    }
}

void SubscriptionManager::functionalCallback(const char *topic, const char *payload)
{

    String callId = getCallId(topic);
    String key = getTopicKey(topic);
    auto it = functionCallbacks.find(key.c_str());
    if (it == functionCallbacks.end())
    {
        Serial.println("Function not found.");
        return;
    }
    JsonDocument doc;
    // Call the function if it was found
    int result = it->second(payload);
    doc["value"] = result;
    doc["key"] = key;
    doc["id"] = deviceId;
    doc["request"] = callId;
    String resultStr;
    serializeJson(doc, resultStr);
    String sendTopic = functionResultsTopic + "/" + key + "/" + callId;
    if (!publishTopic(sendTopic, resultStr))
    {
        Serial.println("Failed to publish result");
    }
}

void SubscriptionManager::variable(const char *name, int *var)
{
    VariableEntry entry(VariableType::INT);
    entry.data.intPtr = var;
    variableRegistry[name] = entry;
    variableCount++;
}

void SubscriptionManager::variable(const char *name, long *var)
{
    VariableEntry entry(VariableType::LONG);
    entry.data.longPtr = var;
    variableRegistry[name] = entry;
    variableCount++;
}

void SubscriptionManager::variable(const char *name, String *var)
{
    VariableEntry entry(VariableType::STRING);
    entry.data.stringPtr = var;
    variableRegistry[name] = entry;
    variableCount++;
}

void SubscriptionManager::variable(const char *name, double *var)
{
    VariableEntry entry(VariableType::DOUBLE);
    entry.data.doublePtr = var;
    variableRegistry[name] = entry;
    variableCount++;
}

// void SubscriptionManager::variable()
// {
//     Serial.println("Variable");
// }

bool SubscriptionManager::publishTopic(String topic, String payload)
{
    if (!processor.isConnected())
    {
        return false;
    }
    return processor.publish(topic.c_str(), payload.c_str());
}

void SubscriptionManager::sendRegistry()
{
    size_t functionCountSubtract = 0;
    for (int i = 0; i < functionCount; i++)
    {
        JsonDocument doc;
        doc["key"] = functionTopics[i].c_str();
        doc["id"] = deviceId;
        String resultStr;
        serializeJson(doc, resultStr);
        if (!publishTopic(functionRegistrationTopic, resultStr.c_str()))
        {
            break;
        }
        functionCountSubtract++;
    }
    functionCount -= functionCountSubtract;
    if (functionCount)
    {
        return;
    }
    functionCountSubtract = 0;
    for (auto &entry : variableRegistry)
    {
        JsonDocument doc;
        doc["key"] = entry.first.c_str();
        doc["id"] = deviceId;
        String resultStr;
        serializeJson(doc, resultStr);
        if (!publishTopic(variableRegistrationTopic, resultStr.c_str()))
        {
            break;
        }
        functionCountSubtract++;
    }

    variableCount -= functionCountSubtract;

    if (variableCount || functionCount)
    {
        return;
    }

    tick.detach();
}

void SubscriptionManager::loop()
{
    processor.loop();
    if (!functionCount || !checkReady)
    {
        return;
    }
    sendRegistry();
}