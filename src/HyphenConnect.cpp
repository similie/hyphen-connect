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

bool HyphenConnect::setup(int logLevel)
{
    loggingLevel = logLevel;
    logger.start(logLevel);
    // bool ok = manager.init();
    // if (!ok)
    // {
    //     Log.errorln("HyphenConnect::setup: manager.init() failed");
    //     return false;
    // }
    if (!initialSetup)
    {
#ifdef HYPHEN_THREADED
        // Launch runner task once
        runner.begin(this, logLevel);
#endif
        initialSetup = true;
    }

#ifdef HYPHEN_THREADED
    // Tell the runner to perform manager.init()
    return runner.initRequested();
#else
    // Single-threaded: do it right now
    return manager.init();
#endif
}

GPSData HyphenConnect::getLocation()
{
    return connection.getLocation();
}

void HyphenConnect::loop()
{
#ifdef HYPHEN_THREADED
    // Runner task is driving manager.loop() on core 0

    // Periodically verify the runner is still alive
    if (!threadCheckReady())
    {
        return;
    }
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
    return manager.publishTopic(topic, payload);
}

void HyphenConnect::function(const char *name,
                             std::function<int(const char *)> fn)
{
    manager.function(name, fn);
}

void HyphenConnect::variable(const char *name, int *v) { manager.variable(name, v); }
void HyphenConnect::variable(const char *name, long *v) { manager.variable(name, v); }
void HyphenConnect::variable(const char *name, String *v) { manager.variable(name, v); }
void HyphenConnect::variable(const char *name, double *v) { manager.variable(name, v); }

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
    // Tear down MQTT/SSL and network
    processor.disconnect();
    connection.disconnect();
    // Stop the runner task
    runner.stop();
#else
    processor.disconnect();
    connection.disconnect();
#endif
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

    runner.stop();
    delay(100);

    // Then bring it back up
    connection.on();
    runner.begin(this, loggingLevel);
    Log.infoln("Runner task restarted.");

    rebuildingThread = false;
}
#endif
// #ifdef HYPHEN_THREADED
// #include "HyphenRunner.h"
// #endif

// HyphenConnect::HyphenConnect(ConnectionType mode)
//     : connection(mode), processor(connection), manager(processor)

// {
// }

// bool HyphenConnect::setup(int logLevel)
// {
//     loggingLevel = logLevel;
//     logger.start(logLevel);
//     if (!initialSetup)
//     {
// #ifdef HYPHEN_THREADED
//         // now that FreeRTOS is running, start the runner:
//         runner.begin(this, logLevel);
// #endif
//         initialSetup = true;
//     }

// #ifdef HYPHEN_THREADED
//     // enqueue the real init request
//     return runner.enqueueSetup(logLevel);
// #else
//     return manager.init();
// #endif
// }

// void HyphenConnect::loop()
// {
// #ifdef HYPHEN_THREADED
//     // nothing—runner is driving manager.loop()
//     if (!threadCheckReady())
//     {
//         return;
//     }
//     Log.infoln("Checking runner task status...");
//     if (runner.isAlive())
//     {
//         return;
//     }

//     Log.errorln("Runner task is not alive. Restarting...");
//     // Option A: fallback to single-threaded
//     rebuildThread();
//     // runner.begin(this, loggingLevel);
//     // Log.infoln("Runner task restarted.");
//     // Option B: actually restart the runner task
//     // runner.begin(this, currentLogLevel);
// #else
//     manager.loop();
// #endif
// }

// bool HyphenConnect::subscribe(const char *topic, std::function<void(const char *, const char *)> cb)
// {
// #ifdef HYPHEN_THREADED
//     return runner.enqueueSubscribe(topic, cb);
// #else
//     return manager.subscribe(topic, cb);
// #endif
// }

// bool HyphenConnect::unsubscribe(const char *topic)
// {
// #ifdef HYPHEN_THREADED
//     return runner.enqueueUnsubscribe(topic);
// #else
//     return manager.unsubscribe(topic);
// #endif
// }

// bool HyphenConnect::publishTopic(const String &topic, const String &payload)
// {
// #ifdef HYPHEN_THREADED
//     return manager.publishTopic(topic, payload);
//     // bool realResult = false;

//     // // first, enqueue the command
//     // bool queued = runner.enqueuePublish(topic, payload, &realResult);
//     // if (!queued)
//     // {
//     //     // queue was full or timeout
//     //     return false;
//     // }
//     bool published = runner.enqueuePublish(topic, payload); // now realResult holds "did the MQTT publish succeed?"

//     if (published)
//     {
//         Log.infoln("Published topic: %s", topic.c_str());
//     }
//     else
//     {
//         Log.errorln("Failed to publish topic: %s", topic.c_str());
//     }
//     return published;
// #else
//     return manager.publishTopic(topic, payload);
// #endif
// }

// void HyphenConnect::function(const char *name, std::function<int(const char *)> fn)
// {
// #ifdef HYPHEN_THREADED
//     runner.enqueueFunction(name, fn);
// #else
//     manager.function(name, fn);
// #endif
// }

// void HyphenConnect::variable(const char *name, int *var)
// {
// #ifdef HYPHEN_THREADED
//     runner.enqueueVariable(name, var);
// #else
//     manager.variable(name, var);
// #endif
// }

// void HyphenConnect::variable(const char *name, long *var)
// {
// #ifdef HYPHEN_THREADED
//     runner.enqueueVariable(name, var);
// #else
//     manager.variable(name, var);
// #endif
// }

// void HyphenConnect::variable(const char *name, String *var)
// {
// #ifdef HYPHEN_THREADED
//     runner.enqueueVariable(name, var);
// #else
//     manager.variable(name, var);
// #endif
// }

// void HyphenConnect::variable(const char *name, double *var)
// {
// #ifdef HYPHEN_THREADED
//     runner.enqueueVariable(name, var);
// #else
//     manager.variable(name, var);
// #endif
// }

// bool HyphenConnect::isConnected()
// {
// #ifdef HYPHEN_THREADED
//     return runner.isConnected();
// #else
//     return manager.isConnected();
// #endif
// }

// void HyphenConnect::disconnect()
// {
// #ifdef HYPHEN_THREADED
//     runner.enqueueDisconnect();
// #else
//     processor.disconnect();
// #endif
// }

// #ifdef HYPHEN_THREADED

// bool HyphenConnect::threadCheckReady()
// {
//     bool ready = !rebuildingThread && (millis() - threadCheck > THREAD_CHECK_INTERVAL);
//     if (ready)
//     {
//         threadCheck = millis();
//     }

//     return ready;
// }

// void HyphenConnect::rebuildThread()
// {
//     rebuildingThread = true;
//     if (processor.isConnected())
//     {
//         processor.disconnect();
//     }

//     if (connection.isConnected())
//     {
//         connection.disconnect();
//     }
//     connection.off();
//     runner.teardownRunnerTask();
//     runner.begin(this, loggingLevel);
//     Log.infoln("Runner task restarted.");
//     rebuildingThread = false;
// }
// #endif

// void HyphenConnect::printCore1StackUsage()
// {
//     TaskHandle_t runnerHandle = HyphenRunner::get().getTaskHandle();
//     // how many words have *never* been used
//     UBaseType_t wordsFree = uxTaskGetStackHighWaterMark(runnerHandle);
//     // convert to bytes
//     size_t bytesFree = wordsFree * sizeof(StackType_t);
//     // you know how many bytes you allocated, e.g. 16384
//     const size_t allocated = 8192; // 16384;

//     Log.infoln(
//         "Runner stack: allocated=%u, free_min=%u, used_max=%u\n",
//         (unsigned)allocated,
//         (unsigned)bytesFree,
//         (unsigned)(allocated - bytesFree));
// }

// Client &HyphenConnect::getClient() { return connection.getClient(); }
// ConnectionClass HyphenConnect::getConnectionClass() { return connection.getClass(); }
// Connection &HyphenConnect::getConnection() { return connection.connection(); }
// SubscriptionManager &HyphenConnect::getSubscriptionManager() { return manager; }
// ConnectionManager &HyphenConnect::getConnectionManager() { return connection; }
