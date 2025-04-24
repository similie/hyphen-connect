#include "HyphenRunner.h"
#include "HyphenConnect.h"

// transport + MQTT bits
static const EventBits_t BIT_TRANSPORT = (1 << 0);
static const EventBits_t BIT_MQTT = (1 << 1);

HyphenRunner &HyphenRunner::get()
{
    static HyphenRunner inst;
    return inst;
}

HyphenRunner::HyphenRunner()
    : taskHandle(nullptr), cmdQ(nullptr), eg(nullptr), hyphen(nullptr)
{
}

void HyphenRunner::begin(HyphenConnect *hc, int logLevel)
{
    hyphen = hc; // point at your wrapper
    cmdQ = xQueueCreate(16, sizeof(Command));
    eg = xEventGroupCreate();

    // spawn on core 1 with plenty of stack
    xTaskCreatePinnedToCore(
        taskEntry, "HyphenRunner", 16000,
        this, tskIDLE_PRIORITY + 1, &taskHandle, 1);

    enqueueSetup(logLevel);
}

bool HyphenRunner::isConnected() const
{
    EventBits_t bits = xEventGroupGetBits(eg);
    return (bits & BIT_TRANSPORT) && (bits & BIT_MQTT);
}

// enqueue helpers
bool HyphenRunner::enqueueSetup(int lvl)
{
    Command c;
    c.type = Command::SETUP;
    c.logLevel = lvl;
    return xQueueSend(cmdQ, &c, 0) == pdTRUE;
}
bool HyphenRunner::enqueueSubscribe(const char *t,
                                    std::function<void(const char *, const char *)> cb)
{
    Command c;
    c.type = Command::SUBSCRIBE;
    c.topic = t;
    c.cb = cb;
    return xQueueSend(cmdQ, &c, 0) == pdTRUE;
}
bool HyphenRunner::enqueueFunction(const char *n,
                                   std::function<int(const char *)> fn)
{
    Command c;
    c.type = Command::FUNCTION;
    c.name = n;
    c.fn = fn;
    return xQueueSend(cmdQ, &c, 0) == pdTRUE;
}
bool HyphenRunner::enqueueVariable(const char *n, void *ptr)
{
    Command c;
    c.type = Command::VARIABLE;
    c.name = n;
    c.ptr = ptr;
    return xQueueSend(cmdQ, &c, 0) == pdTRUE;
}
bool HyphenRunner::enqueuePublish(const String &t, const String &p)
{
    Command c;
    c.type = Command::PUBLISH;
    c.topic = t;
    c.payload = p;
    return xQueueSend(cmdQ, &c, 0) == pdTRUE;
}
void HyphenRunner::enqueueDisconnect()
{
    Command c;
    c.type = Command::DISCONNECT;
    xQueueSend(cmdQ, &c, 0);
}

// static entry
void HyphenRunner::taskEntry(void *pv)
{
    static_cast<HyphenRunner *>(pv)->runTask();
}

void HyphenRunner::runTask()
{
    Command cmd;

    // ** first** do the setup
    if (xQueueReceive(cmdQ, &cmd, portMAX_DELAY) && cmd.type == Command::SETUP)
    {
        // call the real manager.init() directly
        while (!hyphen->manager.init())
        {
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
        // signal transport & MQTT
        xEventGroupSetBits(eg, BIT_TRANSPORT | BIT_MQTT);
    }

    // **then** enter the infinite loop
    for (;;)
    {
        // drain all queued commands
        while (xQueueReceive(cmdQ, &cmd, 0))
        {
            switch (cmd.type)
            {
            case Command::SUBSCRIBE:
                hyphen->manager.subscribe(cmd.topic.c_str(), cmd.cb);
                break;
            case Command::FUNCTION:
                hyphen->manager.function(cmd.name.c_str(), cmd.fn);
                break;
            case Command::VARIABLE:
                hyphen->manager.variable(cmd.name.c_str(),
                                         (long *)cmd.ptr);
                break;
            case Command::PUBLISH:
                hyphen->manager.publishTopic(cmd.topic, cmd.payload);
                break;
            case Command::DISCONNECT:
                hyphen->processor.disconnect();
                break;
            default:
                break;
            }
        }

        // drive the non-blocking loop
        hyphen->manager.loop();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}