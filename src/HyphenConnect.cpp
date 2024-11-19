#include "HyphenConnect.h"

/**
 * @brief Construct a new Hyphen Connect:: Hyphen Connect object
 *
 * @param ConnectionType connectionType
 */
HyphenConnect::HyphenConnect(ConnectionType connectionType) : connection(connectionType), processor(connection), manager(processor)
{
}

HyphenConnect::HyphenConnect() : HyphenConnect(ConnectionType::WIFI_PREFERRED)
{
}
/**
 * @brief This should be called in your setup function.
 *
 * @param loglevel
 * @return true - if connection request is successful
 */
bool HyphenConnect::setup(int loglevel)
{
    /**
     * @brief run these functions once at startup
     */
    if (!initialSetup)
    {
        logger.start(loglevel);
        initialSetup = true;
    }

    return manager.init();
}

/**
 * @brief This should be called in your setup function. The default log level is LOG_LEVEL_SILENT
 *
 * @return true - if connection request is successful
 */
bool HyphenConnect::setup()
{
    return setup(LOG_LEVEL_SILENT);
}

/**
 * @brief This should be called in every loop cycle. This function should not contain any blocking code.
 *
 */
void HyphenConnect::loop()
{
    return manager.loop();
}

/**
 * @brief subscribes to an MQTT topic
 *
 * @param const char * topic
 * @param std::function<void(const char *, const char *)> callback
 * @return true - if the subscription is successful
 */
bool HyphenConnect::subscribe(const char *topic, std::function<void(const char *, const char *)> callback)
{
    return manager.subscribe(topic, callback);
}

/**
 * @brief a function is an that can be attached to an MQTT topic and called from the cloud
 * You function should return an int and accept a const char * as a parameter.
 * the request will look like:
 * /Hy/Post/Function/<DeviceID>/<FunctionID>/<CallID>
 * the results will return to
 * /Hy/Post/Function/Result/<DeviceID>/<FunctionID>/<CallID>
 * @param topic
 * @param callback
 */
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

/**
 * @brief public to a topic in the cloud
 *
 * @param String topic
 * @param String payload
 * @return true - if the message is published
 */
bool HyphenConnect::publishTopic(String topic, String payload)
{
    return manager.publishTopic(topic, payload);
}

/**
 * @brief Is HyphenConnect connected to the network
 *
 * @return true - if connected
 */
bool HyphenConnect::isConnected()
{
    return manager.isConnected();
}

/**
 * @brief Disconnect from the network
 */
void HyphenConnect::disconnect()
{
    return processor.disconnect();
}

/**
 * @brief Returns the connected client
 *
 * @return Client&
 */
Client &HyphenConnect::getClient()
{
    return *connection.getClient();
}

/**
 * @brief Returns the connection class enumeration
 *
 * @return ConnectionClass
 */
ConnectionClass HyphenConnect::getConnectionClass()
{
    return connection.getClass();
}
/**
 * @brief To retrieve the underlying Connection layer
 *
 * @return Connection&
 */
Connection &HyphenConnect::getConnection()
{
    return connection.connection();
}