#include "HyphenConnect.h"

HyphenConnect::HyphenConnect(ConnectionType connectionType) : connection(connectionType), processor(connection), manager(processor)
{
}

HyphenConnect::HyphenConnect() : HyphenConnect(ConnectionType::WIFI_PREFERRED)
{
}

bool HyphenConnect::setup(int loglevel)
{

    /**
     * @brief run these functions once at startup
     *
     */
    if (!initialSetup)
    {
        Serial.begin(UART_BAUD);
        Log.begin(loglevel, &Serial);
        initialSetup = true;
    }

    return manager.init();
}

bool HyphenConnect::setup()
{
    return setup(LOG_LEVEL_SILENT);
}

void HyphenConnect::loop()
{
    return manager.loop();
}

bool HyphenConnect::subscribe(const char *topic, std::function<void(const char *, const char *)> callback)
{
    return manager.subscribe(topic, callback);
}

void HyphenConnect::function(const char *topic, std::function<int(const char *)> callback)
{
    return manager.function(topic, callback);
}
/**
 * @brief pin a variable value as a cloud variable
 * // example
 * hyphen.variable(name, &var);
 * @param const char * name
 * @param int* var
 */
void HyphenConnect::variable(const char *name, int *var)
{
    return manager.variable(name, var);
}
/**
 * @brief pin a variable value as a cloud variable
 * // example
 * hyphen.variable(name, &var);
 * @param const char * name
 * @param long* var
 */
void HyphenConnect::variable(const char *name, long *var)
{
    return manager.variable(name, var);
}
/**
 * @brief pin a variable value as a cloud variable
 * // example
 * hyphen.variable(name, &var);
 * @param const char * name
 * @param String* var
 */
void HyphenConnect::variable(const char *name, String *var)
{
    return manager.variable(name, var);
}
/**
 * @brief pin a variable value as a cloud variable
 * // example
 * hyphen.variable(name, &var);
 * @param const char * name
 * @param double* var
 */
void HyphenConnect::variable(const char *name, double *var)
{
    return manager.variable(name, var);
}

bool HyphenConnect::publishTopic(String topic, String payload)
{
    return manager.publishTopic(topic, payload);
}

bool HyphenConnect::isConnected()
{
    return manager.isConnected();
}

void HyphenConnect::disconnect()
{
    return processor.disconnect();
}
