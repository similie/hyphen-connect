#ifndef COMMUNICATION_REGISTRY_H
#define COMMUNICATION_REGISTRY_H
#include <Arduino.h>
#include <unordered_map>
#include <functional>
#include <array>

class CommunicationRegistry
{

public:
    // Delete copy constructor and assignment operator to prevent copying
    static const size_t CALLBACK_SIZE = 20;
    CommunicationRegistry(const CommunicationRegistry &) = delete;
    CommunicationRegistry &operator=(const CommunicationRegistry &) = delete;
    static CommunicationRegistry &getInstance();
    // Method to register a callback for a specific topic
    bool registerCallback(const std::string &topic, std::function<void(const char *, const char *)> callback);
    // Method to trigger all callbacks for a specific topic
    void triggerCallbacks(const std::string &topic, const char *payload);
    bool hasCallback(const std::string &topic);
    const std::array<std::string, CALLBACK_SIZE> &getCallbacks();
    size_t getCallbackCount();
    void iterateCallbacks(std::function<void(const char *)> callback);

private:
    static CommunicationRegistry instance;
    // std::function<void(char *, byte *, unsigned int)> mqttCallbackFunction;
    std::array<std::string, CALLBACK_SIZE> callbacks;
    size_t callbackCount = 0;
    u_int8_t hasWildCard(String);
    String callbackName(String);
    bool matchCallback(String, String);
    int callbackIndex(const std::string &topic);
    void addCallback(const std::string &topic);
    CommunicationRegistry();
    std::unordered_map<std::string, std::array<std::function<void(const char *, const char *)>, 3>> topicCallbacks;
    // Destructor is private to control destruction
    ~CommunicationRegistry() = default;
};

#endif // COMMUNICATION_REGISTRY_H