#include "managers/CommunicationRegistry.h"

CommunicationRegistry::CommunicationRegistry()
{
}

const std::array<std::string, CommunicationRegistry::CALLBACK_SIZE> &CommunicationRegistry::getCallbacks()
{
    return callbacks;
}

void CommunicationRegistry::iterateCallbacks(std::function<void(const char *)> callback)
{
    for (size_t i = 0; i < callbackCount; i++)
    {
        const char *topic = callbacks[i].c_str();
        if (!topic)
        {
            continue;
        }
        callback(topic);
    }
}

u_int8_t CommunicationRegistry::hasWildCard(String topic)
{
    bool wild = topic.indexOf("#") != -1 || topic.indexOf("+") != -1;
    if (!wild)
    {
        return 0;
    }
    if (topic.indexOf("+") != -1)
    {
        return 1;
    }
    return 2;
}

String CommunicationRegistry::callbackName(String cbName)
{
    bool isWild = hasWildCard(cbName);
    if (isWild == 0)
    {
        return cbName;
    }

    int lastSlash = cbName.lastIndexOf("/");
    return cbName.substring(0, lastSlash);
}

bool CommunicationRegistry::matchCallback(String callback, String topic)
{
    if (callback == topic)
    {
        return true;
    }

    if (hasWildCard(callback) == 0)
    {
        return false;
    }

    String cbName = callbackName(callback);
    return topic.startsWith(cbName);
}

int CommunicationRegistry::callbackIndex(const std::string &topic)
{
    int index = -1;
    for (size_t i = 0; i < callbackCount; i++)
    {
        if (matchCallback(String(callbacks[i].c_str()), String(topic.c_str())))
        {
            index = i;
            break;
        }
    }
    return index;
}
bool CommunicationRegistry::hasCallback(const std::string &topic)
{
    int index = callbackIndex(topic);
    return index != -1;
}

size_t CommunicationRegistry::getCallbackCount()
{
    return callbackCount;
}

CommunicationRegistry &CommunicationRegistry::getInstance()
{
    static CommunicationRegistry instance; // Guaranteed to be lazy-initialized and destroyed correctly
    return instance;
}

void CommunicationRegistry::addCallback(const std::string &topic)
{
    if (hasCallback(topic))
    {
        return;
    }

    callbacks[callbackCount] = topic;
    callbackCount++;
}

bool CommunicationRegistry::registerCallback(const std::string &topic, std::function<void(const char *, const char *)> callback)
{
    addCallback(topic);
    auto &callbacksArray = topicCallbacks[topic]; // Get or create the array for this topic
    // Find the first empty slot in the array
    for (auto &cb : callbacksArray)
    {
        if (!cb)
        { // If this slot is empty
            cb = callback;
            return true;
        }
    }

    Serial.printf("No space to register more callbacks for topic: %s\n", topic.c_str());
    return false; // No empty slots available
}

// Triggers all registered callbacks for a specific topic
void CommunicationRegistry::triggerCallbacks(const std::string &topic, const char *payload)
{
    int index = callbackIndex(topic);
    const std::string searchTopic = callbacks[index];
    auto it = topicCallbacks.find(searchTopic);
    // If there are registered callbacks for this topic, execute them
    if (it != topicCallbacks.end())
    {
        for (const auto &cb : it->second)
        {
            if (cb)
            { // If callback is set
                cb(topic.c_str(), payload);
            }
        }
    }
    else
    {
        Serial.printf("No callbacks registered for topic: %s\n", topic.c_str());
    }
}