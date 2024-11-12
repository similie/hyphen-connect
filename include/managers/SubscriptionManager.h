#ifndef __subscription_manager__
#define __subscription_manager__
#include <ArduinoLog.h>
#include "processors/Processor.h"
#include <ArduinoJson.h>
#include <unordered_map>
#include <array>
#include "Ticker.h"

// Define custom hash and equal functions for Arduino String
struct StringHash
{
    std::size_t operator()(const String &s) const
    {
        return std::hash<std::string>()(s.c_str());
    }
};

struct StringEqual
{
    bool operator()(const String &lhs, const String &rhs) const
    {
        return lhs == rhs;
    }
};

enum class VariableType
{
    INT,
    LONG,
    STRING,
    DOUBLE
};

struct VariableEntry
{
    VariableType type;
    union Data
    {
        int *intPtr;
        long *longPtr;
        String *stringPtr;
        double *doublePtr;

        Data() : intPtr(nullptr) {} // Default initialize to nullptr
    } data;

    // Default constructor (required by std::unordered_map)
    VariableEntry() : type(VariableType::INT), data() {}

    // Constructor that accepts a VariableType
    VariableEntry(VariableType t) : type(t), data() {}
};

class SubscriptionManager
{

public:
    SubscriptionManager(Processor &);
    bool subscribe(const char *topic, std::function<void(const char *, const char *)> callback);
    void function(const char *topic, std::function<int(const char *)> callback);
    // void variable();
    void loop();
    bool init();
    bool publishTopic(String topic, String payload);
    bool isConnected();
    void variable(const char *name, int *var);
    void variable(const char *name, long *var);
    void variable(const char *name, String *var);
    void variable(const char *name, double *var);
    // Hy/Post/Function/<DeviceID>/<FunctionID>/<CallID>
    // Hy/Post/Function/Result/<DeviceID>/<FunctionID>/<CallID>/
    // Hy/Post/Function/Register/<DeviceID>
    // Hy/Post/Variables/Register/<DeviceID>

private:
    Ticker tick;
    const String deviceId = String(DEVICE_PUBLIC_ID);
    bool readySend = false;
    bool checkReady = false;
    void setReady();
    void registerFunctions();
    void sendRegistry();
    String getCallId(const char *);
    String getTopicKey(const char *);
    std::array<std::string, 20> functionTopics;
    u_int8_t functionCount = 0;
    u_int8_t variableCount = 0;
    std::unordered_map<String, VariableEntry, StringHash, StringEqual> variableRegistry;
    std::unordered_map<std::string, std::function<int(const char *)>> functionCallbacks;
    void functionalCallback(const char *, const char *);
    void variableCallback(const char *, const char *);
    static void registrationCallback(SubscriptionManager *instance);
    Processor &processor;

    String functionTopic = String(MQTT_TOPIC_BASE) + "Post/Function/" + deviceId + "/#";
    String functionResultsTopic = String(MQTT_TOPIC_BASE) + "Post/Function/Result/" + deviceId;
    String functionRegistrationTopic = String(MQTT_TOPIC_BASE) + "Post/Function/Register/" + deviceId;

    String variableTopic = String(MQTT_TOPIC_BASE) + "Post/Variable/" + deviceId + "/#";
    String variableResultsTopic = String(MQTT_TOPIC_BASE) + "Post/Variable/Result/" + deviceId;
    String variableRegistrationTopic = String(MQTT_TOPIC_BASE) + "Post/Variable/Register/" + deviceId;
};

#endif
