#include "HyphenConnect.h"

#ifdef HYPHEN_THREADED
#include "HyphenRunner.h"
#endif

HyphenConnect::HyphenConnect(ConnectionType mode)
    : connection(mode),
      processor(connection),
      manager(processor)
{
}

void HyphenConnect::staMode()
{
#if defined(HYPHEN_WIFI_AP_ENABLE) && HYPHEN_WIFI_AP_ENABLE == 1
    WiFiConnection &wifi = (WiFiConnection &)connection.getConnection(ConnectionClass::WIFI);
    wifi.on();
#endif
}

void HyphenConnect::staModeOff()
{
#if defined(HYPHEN_WIFI_AP_ENABLE) && HYPHEN_WIFI_AP_ENABLE == 1
    WiFiConnection &wifi = (WiFiConnection &)connection.getConnection(ConnectionClass::WIFI);
    wifi.off();
#endif
}

bool HyphenConnect::connectionOn()
{
    connectedOn = true;
#ifdef HYPHEN_THREADED
    runner.begin(this);
#else
    return manager.init();
#endif
    staMode();
    return true;
}
bool HyphenConnect::connectionOff()
{
    disconnect();
    connection.off();
    staModeOff();
    // this way the next time we do not send the registration event
    runner.noRegistration();
    return true;
}

bool HyphenConnect::pause()
{
    if (pauseProcessor)
    {
        return false;
    }
    processor.disconnect();
    pauseProcessor = true;
    return true;
}
bool HyphenConnect::resume()
{
    if (!pauseProcessor)
    {
        return false;
    }
    pauseProcessor = false;
    return processor.init();
}

bool HyphenConnect::setup(int logLevel)
{

    loggingLevel = logLevel;

    if (!initialSetup)
    {
        logger.start(logLevel);
        initialSetup = true;
    }

#ifdef HYPHEN_THREADED
    return connectionOn();
#endif

#ifndef HYPHEN_THREADED
    connectedOn = true;
    return manager.init();
#endif
}

GPSData HyphenConnect::getLocation()
{
    return connection.getLocation();
}

void HyphenConnect::loop()
{

    if (!connectedOn)
    {
        return;
    }
#ifdef HYPHEN_THREADED
    runner.loop();
#else
    // if our loop even returns true, we are done for this cycle
    if (manager.loop())
    {
        return;
    }
    // otherwise we attempt to restart the services
    disconnect();
    connectionOn = true;
    manager.init();
#endif
}

bool HyphenConnect::subscribe(const char *topic,
                              std::function<void(const char *, const char *)> cb)
{
    return manager.subscribe(topic, cb);
}

bool HyphenConnect::unsubscribe(const char *topic)
{
    return manager.unsubscribe(topic);
}

bool HyphenConnect::publishTopic(const String &topic,
                                 const String &payload)
{
#ifdef HYPHEN_THREADED
    if (!runner.ready())
    {
        return false;
    }
#endif
    if (!connectedOn)
    {
        return false;
    }
    return manager.publishTopic(topic, payload);
}

bool HyphenConnect::publishTopic(const char *topic, uint8_t *buf, size_t length)
{
#ifdef HYPHEN_THREADED
    if (!runner.ready())
    {
        return false;
    }
#endif
    if (!connectedOn)
    {
        return false;
    }
    return manager.publishTopic(topic, buf, length);
}

void HyphenConnect::function(const char *name,
                             std::function<int(const char *)> fn)
{
    manager.function(name, fn);
}

bool HyphenConnect::ready()
{
    return processor.ready() && manager.ready();
}

void HyphenConnect::variable(const char *name, int *v) { manager.variable(name, v); }
void HyphenConnect::variable(const char *name, long *v) { manager.variable(name, v); }
void HyphenConnect::variable(const char *name, String *v) { manager.variable(name, v); }
void HyphenConnect::variable(const char *name, double *v) { manager.variable(name, v); }

bool HyphenConnect::isConnected()
{
    return manager.isConnected();
}

void HyphenConnect::disconnect()
{
    connectedOn = false;
#ifdef HYPHEN_THREADED
    runner.breakConnector();
    coreDelay(600);
#endif
    processor.disconnect();
    connection.disconnect();
}
SecureClient &HyphenConnect::getSecureClient() { return connection.secureClient(); }
Client &HyphenConnect::getClient() { return connection.getClient(); }

SecureClient &HyphenConnect::newSecureClient() { return connection.getNewSecureClient(); }
Client &HyphenConnect::newClient() { return connection.getNewClient(); }
ConnectionClass HyphenConnect::getConnectionClass() { return connection.getClass(); }
Connection &HyphenConnect::getConnection() { return connection.connection(); }
SubscriptionManager &HyphenConnect::getSubscriptionManager() { return manager; }
ConnectionManager &HyphenConnect::getConnectionManager() { return connection; }

#ifdef HYPHEN_THREADED
bool HyphenConnect::threadCheckReady()
{
    unsigned long now = millis();
    if (now - threadCheck < THREAD_CHECK_INTERVAL)
    {
        return false;
    }
    threadCheck = now;
    return true;
}

#endif
