#include "HyphenRunner.h"
#include "HyphenConnect.h"

static const EventBits_t BIT_TRANSPORT = (1 << 0);
static const EventBits_t BIT_MQTT = (1 << 1);

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
    wantInit = true;
    xTaskCreatePinnedToCore(taskEntry,
                            "HyphenRunner",
                            //  allocatedStack,
                            allocatedStack / sizeof(StackType_t),
                            this,
                            tskIDLE_PRIORITY + 2,
                            &loopHandle,
                            1);
}

void HyphenRunner::begin(HyphenConnect *hc)
{
    hyphen = hc;
    startRunnerTask();

    coreDelay(300);
}

bool HyphenRunner::initRequested()
{
    if (wantInit)
    {
        wantInit = false;
        return true;
    }
    return false;
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
    vTaskDelete(loopHandle);
    loopHandle = nullptr;
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

void HyphenRunner::runTask()
{
    Log.infoln(F("[Runner] starting %s"), String(hyphen->connectedOn));
    while (hyphen->connectedOn)
    {
        // do init if requested
        if (wantInit)
        {
            wantInit = false;
            // BLOCK until manager.init() succeeds
            while (!hyphen->manager.isConnected())
            {
                Log.infoln(F("[Runner] manager::init() attempting connection"));
                while (!hyphen->connection.isConnected() && !hyphen->connection.init())
                {
                    Log.infoln(F("[Runner] connection::init() failed, retry in 500 ms"));
                    vTaskDelay(pdMS_TO_TICKS(500));
                }

                while (hyphen->connection.isConnected() && !hyphen->manager.init())
                {
                    Log.infoln(F("[Runner] manager::init() failed, retry in 500 ms"));

                    vTaskDelay(pdMS_TO_TICKS(500));
                }
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }
        hyphen->manager.loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    Log.infoln(F("[Runner] stopping"));
    stop();
}
