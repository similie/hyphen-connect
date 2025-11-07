#include "HyphenRunner.h"
#include "HyphenConnect.h"
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
    if (!connectionStarted)
    {
        return;
    }

    hyphen->manager.loop();
    if (hyphen->manager.maintain())
    {
        return;
    }
    begin();
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

bool HyphenRunner::isStuck(uint32_t timeoutMs) const
{

    if (hyphen->pauseProcessor)
    {
        return false;
    }

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
    coreDelay(300); // wait for the connection to be fully disconnected
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
