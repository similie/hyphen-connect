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
#ifdef HYPHEN_THREADED
        // Launch runner task once
        return connectionOn();
#endif
    }
    else
    {
#ifdef HYPHEN_THREADED
        // Tell the runner to perform manager.init()
        return runner.initRequested();
#endif
    }

#ifndef HYPHEN_THREADED
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
    // Runner task is driving manager.loop() on core 0

    // Periodically verify the runner is still alive
    if (!threadCheckReady())
    {
        return;
    }
    Log.noticeln("Thread check");
    if (!runner.isAlive())
    {
        Log.errorln("Runner task is not alive. Restarting...");
        rebuildThread();
    }
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
    if (!connectedOn)
    {
        return false;
    }
    return manager.publishTopic(topic, payload);
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
    // #ifdef HYPHEN_THREADED
    //     return manager.isConnected();
    // #else
    //     return manager.isConnected();
    // #endif
}

void HyphenConnect::disconnect()
{

#ifdef HYPHEN_THREADED
    // runner.stop();
#endif
    processor.disconnect();
    connection.disconnect();
}

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

void HyphenConnect::rebuildThread()
{
    rebuildingThread = true;

    // First, take everything down
    processor.disconnect();
    connection.disconnect();
    connection.off();
    delay(300);
    runner.stop();

    // Then bring it back up
    connection.on();
    runner.begin(this);
    Log.infoln("Runner task restarted.");

    rebuildingThread = false;
}
#endif
