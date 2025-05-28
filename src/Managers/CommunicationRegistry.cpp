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

    Log.notice(F("No space to register more callbacks for topic: %s" CR), topic.c_str());
    return false; // No empty slots available
}

bool CommunicationRegistry::unregisterCallback(const std::string &topic)
{
    // 1) Find the topic in our flat callbacks[] list
    // if (xSemaphoreTake(_mutex, portMAX_DELAY) != pdTRUE)
    // {
    //     return false;
    // }
    int idx = callbackIndex(topic);
    Serial.printf("Unregistering callback for topic: %s, index: %d\n", topic.c_str(), idx);
    if (idx < 0 || static_cast<size_t>(idx) >= callbackCount)
    {
        // xSemaphoreGive(_mutex);
        // nothing to remove
        return false;
    }

    // 2) Erase from the map of topic → callback‐array
    size_t erased = topicCallbacks.erase(topic);
    if (erased == 0)
    {
        // map didn’t contain it
        return false;
    }

    size_t i = idx;
    // 3) Shift all later entries in the callbacks[] array left by one
    for (i; i + 1 < callbackCount; i++)
    {
        if (callbacks[i + 1].empty())
        {
            // no more entries to shift
            break;
        }
        callbacks[i] = std::move(callbacks[i + 1]);
    }

    if (i + 1 < callbackCount && !callbacks[i + 1].empty())
    {
        callbacks[i + 1].clear();
    }

    --callbackCount;
    // xSemaphoreGive(_mutex);
    return true;
}

// Triggers all registered callbacks for a specific topic
void CommunicationRegistry::triggerCallbacks(const std::string &topic, const char *payload)
{
    int index = callbackIndex(topic);
    if (index < 0 || static_cast<size_t>(index) >= callbackCount)
    {
        Log.notice(F("No callbacks match topic: %s" CR), topic.c_str());
        return;
    }
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
        Log.notice(F("No callbacks registered for topic: %s" CR), topic.c_str());
    }
}