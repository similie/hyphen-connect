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

bool HyphenConnect::connectionOn()
{
    connectedOn = true;
    runner.begin(this);
    return true;
}
bool HyphenConnect::connectionOff()
{
    connectedOn = false;
    disconnect();
    connection.off();
    return true;
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
    // Tell the runner to perform manager.init()
    // return runner.initRequested();

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
    // Single-threaded: we must call the loop ourselves
    manager.loop();
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
#elif
    if (!connectedOn)
    {
        return false;
    }
#endif
    return manager.publishTopic(topic, payload);
}

bool HyphenConnect::publishTopic(const char *topic, uint8_t *buf, size_t length)
{
#ifdef HYPHEN_THREADED
    if (!runner.ready())
    {
        return false;
    }
#elif
    if (!connectedOn)
    {
        return false;
    }
#endif
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

#ifdef HYPHEN_THREADED
    // runner.stop();
#endif
    processor.disconnect();
    connection.disconnect();
    connectedOn = false;
}
SecureClient &HyphenConnect::getSecureClient() { return connection.secureClient(); }
Client &HyphenConnect::getClient() { return connection.getClient(); }
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
