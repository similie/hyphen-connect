#ifndef __subscription_manager__
#define __subscription_manager__
#include <ArduinoLog.h>
#include "processors/Processor.h"
#include <ArduinoJson.h>
#include <unordered_map>
#include <array>
#include "Ticker.h"
#include "managers/CoreDelay.h"
#ifndef REGISTRATION_WAIT_TIME_IN_SECONDS
#define REGISTRATION_WAIT_TIME_IN_SECONDS 20
#endif

#ifndef FUNCTION_COUNT_MAX
#define FUNCTION_COUNT_MAX 20 // maximum number of functions that can be registered
#endif

#ifndef FUNCTION_REGISTRATION_SECONDS
#define FUNCTION_REGISTRATION_SECONDS 10
#endif

#ifndef KEEP_ALIVE_INTERVAL
#define KEEP_ALIVE_INTERVAL 20 // 20 seconds
#endif
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
    bool unsubscribe(const char *topic);
    void function(const char *topic, std::function<int(const char *)> callback);
    bool loop();
    bool init(bool sendRegistration = true);
    bool maintain();
    bool publishTopic(String topic, String payload);
    bool publishTopic(const char *topic, uint8_t *buf, size_t length);
    bool isConnected();
    void setLastAlive() { lastAlive = millis(); }
    void variable(const char *name, int *var);
    void variable(const char *name, long *var);
    void variable(const char *name, String *var);
    void variable(const char *name, double *var);
    bool ready();
    String runFunction(const char *, const char *);
    String runVariable(const char *, const char *);

private:
    Ticker tick;
    Ticker tock;
    const String deviceId = String(DEVICE_PUBLIC_ID);
    bool readySend = false;
    bool checkReady = false;
    bool subscriptionDone = false;
    bool applyRegistration = false;
    bool sendRegistration = true;
    void runRegistration();
    static void runRegistrationCallback(SubscriptionManager *instance);
    void buildRegistration();
    void toggleRegistration();
    void setReady();
    bool registryReady();
    void registerFunctions();
    void sendRegistry();
    bool keepAliveReady();
    unsigned long lastAlive = 0;
    const unsigned long KEEP_ALIVE_INTERVAL_MS = KEEP_ALIVE_INTERVAL * 1000; // Keep-alive interval in seconds
    String getCallId(const char *);
    String getTopicKey(const char *);
    std::array<std::string, FUNCTION_COUNT_MAX> functionTopics;
    u_int8_t functionCount = 0;
    u_int8_t variableCount = 0;
    std::unordered_map<String, VariableEntry, StringHash, StringEqual> variableRegistry;
    std::unordered_map<std::string, std::function<int(const char *)>> functionCallbacks;
    void functionalCallback(const char *, const char *);
    void variableCallback(const char *, const char *);
    static void registrationCallback(SubscriptionManager *instance);
    Processor &processor;

    String registrationTopic = String(MQTT_TOPIC_BASE) + "Post/Register/" + deviceId;
    String functionTopic = String(MQTT_TOPIC_BASE) + "Post/Function/" + deviceId + "/#";
    String functionResultsTopic = String(MQTT_TOPIC_BASE) + "Post/Function/Result/" + deviceId;
    String variableTopic = String(MQTT_TOPIC_BASE) + "Post/Variable/" + deviceId + "/#";
    String variableResultsTopic = String(MQTT_TOPIC_BASE) + "Post/Variable/Result/" + deviceId;
};

#endif
