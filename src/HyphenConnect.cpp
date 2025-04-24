#include "HyphenConnect.h"

#ifdef HYPHEN_THREADED
#include "HyphenRunner.h"
#endif

HyphenConnect::HyphenConnect(ConnectionType mode)
    : connection(mode), processor(connection), manager(processor)
#ifdef HYPHEN_THREADED
      ,
      runner(HyphenRunner::get())
#endif
{
    // **no runner.begin here!**
}

bool HyphenConnect::setup(int logLevel)
{
    if (!initialSetup)
    {
        logger.start(logLevel);
#ifdef HYPHEN_THREADED
        // now that FreeRTOS is running, start the runner:
        runner.begin(this, logLevel);
#endif
        initialSetup = true;
    }

#ifdef HYPHEN_THREADED
    // enqueue the real init request
    return runner.enqueueSetup(logLevel);
#else
    return manager.init();
#endif
}

void HyphenConnect::loop()
{
#ifdef HYPHEN_THREADED
    // nothing—runner is driving manager.loop()
#else
    manager.loop();
#endif
}

bool HyphenConnect::subscribe(const char *topic, std::function<void(const char *, const char *)> cb)
{
#ifdef HYPHEN_THREADED
    return runner.enqueueSubscribe(topic, cb);
#else
    return manager.subscribe(topic, cb);
#endif
}

bool HyphenConnect::publishTopic(const String &topic, const String &payload)
{
#ifdef HYPHEN_THREADED
    return runner.enqueuePublish(topic, payload);
#else
    return manager.publishTopic(topic, payload);
#endif
}

void HyphenConnect::function(const char *name, std::function<int(const char *)> fn)
{
#ifdef HYPHEN_THREADED
    runner.enqueueFunction(name, fn);
#else
    manager.function(name, fn);
#endif
}

void HyphenConnect::variable(const char *name, int *var)
{
#ifdef HYPHEN_THREADED
    runner.enqueueVariable(name, var);
#else
    manager.variable(name, var);
#endif
}

void HyphenConnect::variable(const char *name, long *var)
{
#ifdef HYPHEN_THREADED
    runner.enqueueVariable(name, var);
#else
    manager.variable(name, var);
#endif
}

void HyphenConnect::variable(const char *name, String *var)
{
#ifdef HYPHEN_THREADED
    runner.enqueueVariable(name, var);
#else
    manager.variable(name, var);
#endif
}

void HyphenConnect::variable(const char *name, double *var)
{
#ifdef HYPHEN_THREADED
    runner.enqueueVariable(name, var);
#else
    manager.variable(name, var);
#endif
}

bool HyphenConnect::isConnected()
{
#ifdef HYPHEN_THREADED
    return runner.isConnected();
#else
    return manager.isConnected();
#endif
}

void HyphenConnect::disconnect()
{
#ifdef HYPHEN_THREADED
    runner.enqueueDisconnect();
#else
    processor.disconnect();
#endif
}

Client &HyphenConnect::getClient() { return *connection.getClient(); }
ConnectionClass HyphenConnect::getConnectionClass() { return connection.getClass(); }
Connection &HyphenConnect::getConnection() { return connection.connection(); }
SubscriptionManager &HyphenConnect::getSubscriptionManager() { return manager; }
ConnectionManager &HyphenConnect::getConnectionManager() { return connection; }
