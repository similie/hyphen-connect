#include "HyphenRunner.h"
#include "HyphenConnect.h"

// static const EventBits_t BIT_TRANSPORT = (1 << 0);
// static const EventBits_t BIT_MQTT = (1 << 1);

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
    eg = xEventGroupCreate();
    // wantInit = true;
    runConnection = true;
    // initialized = false;
    coreDelay(500);
    xTaskCreatePinnedToCore(taskEntry,
                            "HyphenRunner",
                            //  allocatedStack,
                            allocatedStack / sizeof(StackType_t),
                            this,
                            tskIDLE_PRIORITY + 1,
                            &loopHandle,
                            0);
}

bool HyphenRunner::requiresRebuild()
{
    // we check every MQTT_KEEP_ALIVE_INTERVAL seconds
    if (millis() - threadCheck < MQTT_KEEP_ALIVE_INTERVAL * 1000)
    {
        return false;
    }
    Log.noticeln(F("[Runner] checking if requires rebuild..."));
    threadCheck = millis();
    return !isAlive() || isStuck(LAST_ALIVE_THRESHOLD);
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

    if (connectionStarted)
    {
        hyphen->manager.loop();
    }

    if (requiresRebuild())
    {
        return rebuildConnection();
    }
}

void HyphenRunner::begin(HyphenConnect *hc)
{
    hyphen = hc;
    // initialized = false;
    connectionStarted = false;
    runConnection = true;
    startRunnerTask();
    coreDelay(300);
}

bool HyphenRunner::isAlive() const
{
    if (!loopHandle)
        return false;
    return eTaskGetState(loopHandle) != eDeleted;
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
        // no code after this!
    }
    else
    {
        // another task is tearing us down
        TaskHandle_t h = loopHandle;
        loopHandle = nullptr;
        vTaskDelete(h);
    }
    // xTaskAbortDelay(loopHandle);
    // vTaskDelete(loopHandle);
    // loopHandle = nullptr;
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

bool HyphenRunner::isStuck(uint32_t timeoutMs) const
{
    Log.noticeln("Checking if stuck... %lu", (millis() - _lastAliveMs));
    return (millis() - _lastAliveMs) > timeoutMs;
}

bool HyphenRunner::initManager()
{
    if (connectionStarted)
    {
        return false;
    }
    while (!hyphen->manager.isConnected() && runConnection)
    {
        while (!hyphen->connection.isConnected() && !hyphen->connection.init() && runConnection)
        {
            Log.infoln(F("[Runner] connection::init() failed, retry in 500 ms"));
            coreDelay(500);
            tick();
        }
        tick();
        coreDelay(500);
        Log.infoln(F("[Runner] connection::init() succeeded"));
        while (hyphen->connection.isConnected() && !hyphen->manager.init() && runConnection)
        {
            Log.infoln(F("[Runner] manager::init() failed, retry in 500 ms"));
            coreDelay(500);
            tick();
        }
        Log.infoln(F("[Runner] manager::init() succeeded"));
        tick();
    }
    connectionStarted = true;
    return true;
}

void HyphenRunner::rebuildConnection()
{
    Log.infoln(F("[Runner] rebuilding connection..."));
    runConnection = false;

    coreDelay(600); // wait for the connection to be fully disconnected
    stop();
    coreDelay(100);
    hyphen->processor.disconnect();
    hyphen->connection.disconnect();
    hyphen->connection.off();
    Log.infoln(F("[Runner] connection::off()"));
    connectionStarted = false;
    coreDelay(100);
    startRunnerTask();
    Log.infoln(F("[Runner] connection::on()"));
}

void HyphenRunner::runTask()
{
    Log.infoln(F("[Runner] starting %s"), String(hyphen->connectedOn));
    connectionStarted = false;
    while (hyphen->connectedOn && runConnection)
    {

        if (initManager())
        {
            Log.infoln(F("[Runner] connection started"));
        }
        hyphen->manager.maintain();
        tick();
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    Log.infoln(F("[Runner] stopping"));
    stop();
}
