#include "HyphenRunner.h"
#include "HyphenConnect.h"
#include "Backoff.h"

// Reconnect backoff bounds. Attempts are unlimited (a field device must keep
// trying), but the delay between them grows exponentially from BASE up to MAX so
// an outage doesn't become a reconnect storm against the broker/modem.
#ifndef HYPHEN_RECONNECT_BACKOFF_BASE_MS
#define HYPHEN_RECONNECT_BACKOFF_BASE_MS 500
#endif
#ifndef HYPHEN_RECONNECT_BACKOFF_MAX_MS
#define HYPHEN_RECONNECT_BACKOFF_MAX_MS 30000
#endif
HyphenRunner &HyphenRunner::get()
{
    static HyphenRunner inst;
    return inst;
}

HyphenRunner::HyphenRunner()
{
}

void HyphenRunner::startRunnerTask()
{
    runConnection = true;
    coreDelay(500);
    xTaskCreatePinnedToCore(taskEntry,
                            "HyphenRunner",
                            allocatedStack / sizeof(StackType_t),
                            this,
                            tskIDLE_PRIORITY + 1,
                            &loopHandle,
                            0);
}

bool HyphenRunner::ready()
{
    return hyphen->connectedOn && connectionStarted;
}
void HyphenRunner::loop()
{
    if (!ready())
    {
        return;
    }
    processorLoops();
}

void HyphenRunner::processorLoops()
{
    if (!connectionStarted || hyphen->isPaused())
    {
        return;
    }
    if (hyphen->manager.loop())
    {
        return;
    }
    Log.infoln(F("[Runner] processorLoops detected connection issue, rebuilding..."));
    rebuildConnection();
}

void HyphenRunner::begin()
{
    connectionStarted = false;
    startRunnerTask();
    coreDelay(300);
}

void HyphenRunner::begin(HyphenConnect *hc)
{
    hyphen = hc;
    begin();
}

void HyphenRunner::stop()
{
    if (!loopHandle)
    {
        return;
    }
    TaskHandle_t me = xTaskGetCurrentTaskHandle();

    if (me == loopHandle)
    {
        // we’re _inside_ the runner, so delete ourselves
        loopHandle = nullptr;
        vTaskDelete(NULL);
        return;
    }

    // another task is tearing us down
    TaskHandle_t h = loopHandle;
    loopHandle = nullptr;
    vTaskDelete(h);
}

void HyphenRunner::restart(HyphenConnect *hc)
{
    stop();
    begin(hc);
}

void HyphenRunner::taskEntry(void *pv)
{
    static_cast<HyphenRunner *>(pv)->runTask();
}

bool HyphenRunner::initManager()
{
    if (connectionStarted)
    {
        return false;
    }
    unsigned long bringupStart = millis();
    while (!hyphen->manager.isConnected() && runConnection)
    {
        hyphen->incrementConnectAttempts();
        while (!hyphen->connection.isConnected() && !hyphen->connection.init() && runConnection)
        {
            unsigned long backoff = hyphen::backoff::delayMs(
                hyphen->getConnectAttempts(), HYPHEN_RECONNECT_BACKOFF_BASE_MS,
                HYPHEN_RECONNECT_BACKOFF_MAX_MS);
            Log.infoln(F("[Runner] connection::init() failed, retry in %lu ms"), backoff);
            coreDelay(backoff);
            hyphen->incrementConnectAttempts();
        }
        coreDelay(500);
        Log.infoln(F("[Runner] connection::init() succeeded"));
        while (hyphen->connection.isConnected() && !hyphen->manager.init(sendRegistration) && runConnection)
        {
            unsigned long backoff = hyphen::backoff::delayMs(
                hyphen->getConnectAttempts(), HYPHEN_RECONNECT_BACKOFF_BASE_MS,
                HYPHEN_RECONNECT_BACKOFF_MAX_MS);
            Log.infoln(F("[Runner] manager::init() failed, retry in %lu ms"), backoff);
            coreDelay(backoff);
            hyphen->incrementConnectAttempts();
            if (!hyphen->connection.maintain())
            {
                hyphen->connection.off();
                break;
            }
        }
        Log.infoln(F("[Runner] manager::init() succeeded"));
    }

    if (runConnection)
    {
        Log.noticeln("[diag] connection established in %lu ms (heap=%u)",
                     millis() - bringupStart, (unsigned)ESP.getFreeHeap());
        hyphen->resetConnectAttempts();
        connectionStarted = true;
#ifndef HYPHEN_REREGISTER_ON_RECONNECT
        // Default: the cloud catalog manifest is published once on first connect
        // and not resent on reconnect. Define HYPHEN_REREGISTER_ON_RECONNECT to
        // re-publish it every time the device re-establishes its MQTT session.
        sendRegistration = false;
#endif
    }

    hyphen->manager.setLastAlive();
    return true;
}

void HyphenRunner::rebuildConnection()
{
    Log.warningln("[diag] rebuild: tearing down + power-cycling (attempts=%u heap=%u)",
                  hyphen->getConnectAttempts(), (unsigned)ESP.getFreeHeap());
    Log.infoln(F("[Runner] rebuilding connection..."));
    breakConnector();
    connectionStarted = false;
    coreDelay(300); // wait for the connection to be fully disconnected
    stop();
    coreDelay(100);
    hyphen->processor.disconnect();
    hyphen->connection.disconnect();
    hyphen->connection.off();
    Log.infoln(F("[Runner] connection::off()"));
    begin();
}

void HyphenRunner::runTask()
{
    Log.infoln(F("[Runner] starting %s"), String(hyphen->connectedOn));
    unsigned long delaycheck = millis();
    connectionStarted = false;
    if (initManager())
    {
        Log.infoln(F("[Runner] connection started"));
    }

    Log.infoln(F("[Runner] stopping"));
    loopHandle = nullptr;
    vTaskDelete(NULL);
}
