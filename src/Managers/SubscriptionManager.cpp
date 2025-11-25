#include "managers/SubscriptionManager.h"

SubscriptionManager::SubscriptionManager(Processor &processor) : processor(processor)
{
}

bool SubscriptionManager::init(bool sendRegistration)
{
    bool init = processor.init();
    if (!init)
    {
        Log.errorln("Failed to initialize processor");
        return false;
    }

    this->sendRegistration = sendRegistration;
    applyRegistration = false;
    if (sendRegistration)
    {
        subscriptionDone = false;
    }
    buildRegistration();
    setLastAlive();
    return init;
}

void SubscriptionManager::runRegistration()
{
    applyRegistration = false;
    tock.detach();
    Log.notice(F("Subscription Manager initialized %s" CR), functionTopic.c_str());
    processor.subscribe(functionTopic.c_str(), [this](const char *topic, const char *payload)
                        { functionalCallback(topic, payload); });

    processor.subscribe(variableTopic.c_str(), [this](const char *topic, const char *payload)
                        { variableCallback(topic, payload); });

    if (!sendRegistration)
    {
        subscriptionDone = true;
        return;
    }
    registerFunctions();
}

void SubscriptionManager::buildRegistration()
{
    tock.attach(FUNCTION_REGISTRATION_SECONDS, &SubscriptionManager::runRegistrationCallback, this);
}

void SubscriptionManager::toggleRegistration()
{
    applyRegistration = true;
}

void SubscriptionManager::runRegistrationCallback(SubscriptionManager *instance)
{
    static_cast<SubscriptionManager *>(instance)->toggleRegistration();
}

bool SubscriptionManager::ready()
{
    return subscriptionDone;
}

bool SubscriptionManager::subscribe(const char *topic, std::function<void(const char *, const char *)> callback)
{
    return processor.subscribe(topic, callback);
}

bool SubscriptionManager::unsubscribe(const char *topic)
{
    return processor.unsubscribe(topic);
}

void SubscriptionManager::registerFunctions()
{
    tick.attach(REGISTRATION_WAIT_TIME_IN_SECONDS, &SubscriptionManager::registrationCallback, this);
}

void SubscriptionManager::registrationCallback(SubscriptionManager *instance)
{
    static_cast<SubscriptionManager *>(instance)->setReady();
}

void SubscriptionManager::setReady()
{
    if (!processor.ready())
    {
        Log.errorln("Connection not ready");
        return;
    }
    checkReady = true;
}

void SubscriptionManager::function(const char *topic, std::function<int(const char *)> callback)
{

    if (functionCount >= FUNCTION_COUNT_MAX)
    {
        Log.warningln("Function limit reached");
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

String SubscriptionManager::runVariable(const char *topic, const char *payload)
{
    String callId = getCallId(topic);
    String key = getTopicKey(topic);
    auto varIt = variableRegistry.find(key.c_str());
    if (varIt == variableRegistry.end())
    {
        Log.warningln("Variable or function not found.");
        return "";
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
    return resultStr;
}

void SubscriptionManager::variableCallback(const char *topic, const char *payload)
{
    String callId = getCallId(topic);
    String key = getTopicKey(topic);
    // Serialize and publish the JSON document
    String resultStr = runVariable(topic, payload);
    String sendTopic = variableResultsTopic + "/" + key + "/" + callId;
    if (!publishTopic(sendTopic, resultStr))
    {
        Log.errorln("Failed to publish variable result");
    }
}

String SubscriptionManager::runFunction(const char *topic, const char *payload)
{
    String callId = getCallId(topic);
    String key = getTopicKey(topic);
    auto it = functionCallbacks.find(key.c_str());
    if (it == functionCallbacks.end())
    {
        Log.warningln("Function not found.");
        return "";
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
    return resultStr;
}

void SubscriptionManager::functionalCallback(const char *topic, const char *payload)
{
    String callId = getCallId(topic);
    String key = getTopicKey(topic);
    String resultStr = runFunction(topic, payload);
    String sendTopic = functionResultsTopic + "/" + key + "/" + callId;
    if (!publishTopic(sendTopic, resultStr))
    {
        Log.errorln("Failed to publish result");
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

bool SubscriptionManager::publishTopic(String topic, String payload)
{
    Log.noticeln("Publishing to %s: %s", topic.c_str(), payload.c_str());
    return processor.publish(topic.c_str(), payload.c_str());
}

bool SubscriptionManager::publishTopic(const char *topic, uint8_t *buf, size_t length)
{
    Log.noticeln("Publishing to %s with length %d", topic, length);
    return processor.publish(topic, buf, length);
}

void SubscriptionManager::sendRegistry()
{
    checkReady = false;
    JsonDocument doc;
    doc["id"] = deviceId;
    JsonArray funArray = doc["functions"].to<JsonArray>();
    JsonArray varArray = doc["variables"].to<JsonArray>();
    doc["functionCount"] = functionCount;
    doc["variableCount"] = variableCount;
    String variables[variableCount];
    for (int i = 0; i < functionCount; i++)
    {
        funArray.add(functionTopics[i]);
    }
    for (auto &entry : variableRegistry)
    {
        varArray.add(entry.first.c_str());
    }
    String resultStr;
    serializeJson(doc, resultStr);
    coreDelay(500);
    if (!publishTopic(registrationTopic, resultStr.c_str()))
    {
        return Log.errorln("Failed to publish registry");
    }
    tick.detach();
    subscriptionDone = true;
}

bool SubscriptionManager::registryReady()
{
    return functionCount > 0 && checkReady;
}

bool SubscriptionManager::maintain()
{
    Log.infoln("Running maintainance");
    return processor.maintain();
}
/**
 * @brief returns true on an interval to keep the connection alive
 *
 * @return true - if the keep alive interval is ready
 */
bool SubscriptionManager::keepAliveReady()
{
    unsigned long now = millis();
    if (now - lastAlive >= KEEP_ALIVE_INTERVAL_MS)
    {
        setLastAlive();
        return true;
    }
    return false;
}

bool SubscriptionManager::loop()
{
    if (applyRegistration)
    {
        runRegistration();
    }

    if (registryReady())
    {
        sendRegistry();
    }

    processor.loop();

    if (!keepAliveReady())
    {
        return true;
    }

    return maintain();
}