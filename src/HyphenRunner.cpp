#include "HyphenRunner.h"
#include "HyphenConnect.h"

static const EventBits_t BIT_TRANSPORT = (1 << 0);
static const EventBits_t BIT_MQTT = (1 << 1);

HyphenRunner &HyphenRunner::get()
{
    static HyphenRunner inst;
    return inst;
}

HyphenRunner::HyphenRunner() {}

void HyphenRunner::begin(HyphenConnect *hc, int logLevel)
{
    hyphen = hc;
    eg = xEventGroupCreate();
    xTaskCreatePinnedToCore(taskEntry,
                            "HyphenRunner",
                            allocatedStack / sizeof(StackType_t),
                            this,
                            tskIDLE_PRIORITY + 1,
                            &taskHandle,
                            0);

    wantInit = true;
    coreDelay(1000);
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

bool HyphenRunner::isConnected() const
{
    EventBits_t b = xEventGroupGetBits(eg);
    return (b & BIT_TRANSPORT) && (b & BIT_MQTT);
}

bool HyphenRunner::isAlive() const
{
    if (!taskHandle)
        return false;
    return eTaskGetState(taskHandle) != eDeleted;
}

void HyphenRunner::stop()
{
    if (taskHandle)
    {
        vTaskDelete(taskHandle);
        taskHandle = nullptr;
    }
}

void HyphenRunner::restart(HyphenConnect *hc, int logLevel)
{
    stop();
    begin(hc, logLevel);
}

void HyphenRunner::taskEntry(void *pv)
{
    static_cast<HyphenRunner *>(pv)->runTask();
}

void HyphenRunner::runTask()
{
    for (;;)
    {
        // Serial.printf("HyphenRunner::runTask() %s", String(wantInit));
        // do init if requested
        if (wantInit)
        {
            // BLOCK until manager.init() succeeds
            while (!hyphen->manager.init())
            {
                while (!hyphen->connection.isConnected() && !hyphen->connection.init())
                {
                    Log.infoln(F("[Runner] connection::init() failed, retry in 500 ms"));
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
                Log.infoln(F("[Runner] manager::init() failed, retry in 500 ms"));

                vTaskDelay(pdMS_TO_TICKS(500));
            }
            // signal “connected”
            xEventGroupSetBits(eg, BIT_TRANSPORT | BIT_MQTT);
            wantInit = false;
        }

        // then drive the loop
        hyphen->manager.loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
// /* HyphenRunner.cpp */
// #include "HyphenRunner.h"
// #include "HyphenConnect.h"
// static const EventBits_t BIT_TRANSPORT = (1 << 0);
// static const EventBits_t BIT_MQTT = (1 << 1);

// HyphenRunner &HyphenRunner::get()
// {
//     static HyphenRunner inst;
//     return inst;
// }

// HyphenRunner::HyphenRunner() {}

// void HyphenRunner::begin(HyphenConnect *hc, int logLevel)
// {
//     hyphen = hc;
//     cmdQ = xQueueCreate(16, sizeof(Command *));
//     eg = xEventGroupCreate();
//     xTaskCreatePinnedToCore(taskEntry, "HyphenRunner", allocatedStack / sizeof(StackType_t), this,
//                             tskIDLE_PRIORITY + 1, &taskHandle, 0);
//     enqueueSetup(logLevel);
// }

// bool HyphenRunner::isAlive() const
// {
//     if (!taskHandle)
//         return false;
//     eTaskState s = eTaskGetState(taskHandle);
//     return s != eDeleted && s != eInvalid;
// }

// bool HyphenRunner::isConnected() const
// {
//     EventBits_t bits = xEventGroupGetBits(eg);
//     return (bits & BIT_TRANSPORT) && (bits & BIT_MQTT);
// }

// bool HyphenRunner::enqueueSetup(int logLevel)
// {
//     Command *cmd = new Command();
//     cmd->type = CommandType::SETUP;
//     cmd->logLevel = logLevel;
//     bool ok = xQueueSend(cmdQ, &cmd, 0) == pdTRUE;
//     if (!ok)
//         delete cmd;
//     return ok;
// }

// bool HyphenRunner::enqueueSubscribe(const char *topic, std::function<void(const char *, const char *)> cb)
// {
//     Command *cmd = new Command();
//     cmd->type = CommandType::SUBSCRIBE;
//     cmd->topic = topic;
//     cmd->cb = cb;
//     bool ok = xQueueSend(cmdQ, &cmd, 0) == pdTRUE;
//     if (!ok)
//         delete cmd;
//     return ok;
// }

// bool HyphenRunner::enqueueUnsubscribe(const char *topic)
// {
//     Command *cmd = new Command();
//     cmd->type = CommandType::UNSUBSCRIBE;
//     cmd->topic = topic;
//     bool ok = xQueueSend(cmdQ, &cmd, 0) == pdTRUE;
//     if (!ok)
//         delete cmd;
//     return ok;
// }

// bool HyphenRunner::enqueuePublish(const String &topic, const String &payload)
// {
//     Command *cmd = new Command();
//     cmd->type = CommandType::PUBLISH;
//     cmd->topic = topic;
//     cmd->payload = payload;
//     cmd->replyQ = xQueueCreate(1, sizeof(bool));
//     if (!cmd->replyQ)
//     {
//         delete cmd;
//         return false;
//     }

//     // 2) Queue the work (wait up to 100 ms)
//     if (xQueueSend(cmdQ, &cmd, pdMS_TO_TICKS(100)) != pdTRUE)
//     {
//         vQueueDelete(cmd->replyQ);
//         delete cmd;
//         return false;
//     }

//     // 3) Wait for the runner to send back the real MQTT result
//     bool realResult = false;
//     if (xQueueReceive(cmd->replyQ, &realResult, pdMS_TO_TICKS(15100)) != pdTRUE)
//     {
//         // timeout → treat as failure
//         realResult = false;
//     }

//     // 4) Cleanup and return
//     vQueueDelete(cmd->replyQ);
//     return realResult;
// }

// bool HyphenRunner::enqueuePublish(const String &topic, const String &payload, bool *outResult)
// {
//     // 1) Build the command + a one‐slot reply queue
//     Command *cmd = new Command();
//     cmd->type = CommandType::PUBLISH;
//     cmd->topic = topic;
//     cmd->payload = payload;
//     cmd->replyQ = xQueueCreate(1, sizeof(bool));
//     if (!cmd->replyQ)
//     {
//         delete cmd;
//         return false;
//     }

//     // 2) Queue the work (up to 100 ms)
//     if (xQueueSend(cmdQ, &cmd, pdMS_TO_TICKS(100)) != pdTRUE)
//     {
//         vQueueDelete(cmd->replyQ);
//         delete cmd;
//         return false;
//     }

//     // 3) Poll for the real MQTT result (up to 5 s), sleeping between tries
//     bool realResult = false;
//     const TickType_t timeout = pdMS_TO_TICKS(16000);
//     const TickType_t pollInterval = pdMS_TO_TICKS(50);
//     TickType_t start = xTaskGetTickCount();
//     while (xTaskGetTickCount() - start < timeout)
//     {
//         // non‐blocking check
//         if (xQueueReceive(cmd->replyQ, &realResult, 0) == pdTRUE)
//         {
//             break; // we got the runner’s reply
//         }
//         coreD(pollInterval);
//     }

//     // 4) clean up and return it
//     vQueueDelete(cmd->replyQ);
//     return realResult;
// }

// bool HyphenRunner::enqueueFunction(const char *name, std::function<int(const char *)> fn)
// {
//     Command *cmd = new Command();
//     cmd->type = CommandType::FUNCTION;
//     cmd->name = name;
//     cmd->fn = fn;
//     bool ok = xQueueSend(cmdQ, &cmd, 0) == pdTRUE;
//     if (!ok)
//         delete cmd;
//     return ok;
// }

// bool HyphenRunner::enqueueVariable(const char *name, int *var)
// {
//     Command *cmd = new Command();
//     cmd->type = CommandType::VARIABLE;
//     cmd->name = name;
//     cmd->ptr = var;
//     cmd->varType = VarType::INT;
//     bool ok = xQueueSend(cmdQ, &cmd, 0) == pdTRUE;
//     if (!ok)
//         delete cmd;
//     return ok;
// }

// bool HyphenRunner::enqueueVariable(const char *name, long *var)
// {
//     Command *cmd = new Command();
//     cmd->type = CommandType::VARIABLE;
//     cmd->name = name;
//     cmd->ptr = var;
//     cmd->varType = VarType::LONG;
//     bool ok = xQueueSend(cmdQ, &cmd, 0) == pdTRUE;
//     if (!ok)
//         delete cmd;
//     return ok;
// }

// bool HyphenRunner::enqueueVariable(const char *name, String *var)
// {
//     Command *cmd = new Command();
//     cmd->type = CommandType::VARIABLE;
//     cmd->name = name;
//     cmd->ptr = var;
//     cmd->varType = VarType::STRING;
//     bool ok = xQueueSend(cmdQ, &cmd, 0) == pdTRUE;
//     if (!ok)
//         delete cmd;
//     return ok;
// }

// bool HyphenRunner::enqueueVariable(const char *name, double *var)
// {
//     Command *cmd = new Command();
//     cmd->type = CommandType::VARIABLE;
//     cmd->name = name;
//     cmd->ptr = var;
//     cmd->varType = VarType::DOUBLE;
//     bool ok = xQueueSend(cmdQ, &cmd, 0) == pdTRUE;
//     if (!ok)
//         delete cmd;
//     return ok;
// }

// void HyphenRunner::enqueueDisconnect()
// {
//     Command *cmd = new Command();
//     cmd->type = CommandType::DISCONNECT;
//     if (xQueueSend(cmdQ, &cmd, 0) != pdTRUE)
//         delete cmd;
// }

// void HyphenRunner::runTask()
// {
//     Command *cmdPtr = nullptr;
//     // initial SETUP
//     if (xQueueReceive(cmdQ, &cmdPtr, portMAX_DELAY) && cmdPtr->type == CommandType::SETUP)
//     {
//         while (!hyphen->manager.init())
//         {
//             while (!hyphen->connection.isConnected() && !hyphen->connection.init())
//                 vTaskDelay(pdMS_TO_TICKS(500));
//             vTaskDelay(pdMS_TO_TICKS(500));
//         }
//         xEventGroupSetBits(eg, BIT_TRANSPORT | BIT_MQTT);
//         delete cmdPtr;
//     }
//     // main loop
//     for (;;)
//     {
//         while (xQueueReceive(cmdQ, &cmdPtr, 0))
//         {
//             switch (cmdPtr->type)
//             {
//             case CommandType::SUBSCRIBE:
//                 hyphen->manager.subscribe(cmdPtr->topic.c_str(), cmdPtr->cb);
//                 break;
//             case CommandType::UNSUBSCRIBE:
//                 hyphen->manager.unsubscribe(cmdPtr->topic.c_str());
//                 break;
//             case CommandType::PUBLISH:
//             {
//                 bool ok = hyphen->manager.publishTopic(cmdPtr->topic, cmdPtr->payload);
//                 if (cmdPtr->replyQ)
//                     xQueueSend(cmdPtr->replyQ, &ok, 0);
//                 break;
//             }
//             case CommandType::FUNCTION:
//                 hyphen->manager.function(cmdPtr->name.c_str(), cmdPtr->fn);
//                 break;
//             case CommandType::VARIABLE:
//                 switch (cmdPtr->varType)
//                 {
//                 case VarType::INT:
//                     hyphen->manager.variable(cmdPtr->name.c_str(), static_cast<int *>(cmdPtr->ptr));
//                     break;
//                 case VarType::LONG:
//                     hyphen->manager.variable(cmdPtr->name.c_str(), static_cast<long *>(cmdPtr->ptr));
//                     break;
//                 case VarType::STRING:
//                     hyphen->manager.variable(cmdPtr->name.c_str(), static_cast<String *>(cmdPtr->ptr));
//                     break;
//                 case VarType::DOUBLE:
//                     hyphen->manager.variable(cmdPtr->name.c_str(), static_cast<double *>(cmdPtr->ptr));
//                     break;
//                 }
//                 break;
//             case CommandType::DISCONNECT:
//                 hyphen->processor.disconnect();
//                 break;
//             default:
//                 break;
//             }
//             delete cmdPtr;
//         }
//         hyphen->manager.loop();
//         vTaskDelay(pdMS_TO_TICKS(10));
//     }
// }

// void HyphenRunner::teardownRunnerTask()
// {
//     if (taskHandle == nullptr)
//     {
//         return;
//     }
//     vTaskDelete(taskHandle);
//     taskHandle = nullptr;
// }

// // #include "HyphenRunner.h"
// // #include "HyphenConnect.h"

// // // which bits in eg() mean “transport up” and “MQTT up”
// // static const EventBits_t BIT_TRANSPORT = (1 << 0);
// // static const EventBits_t BIT_MQTT = (1 << 1);

// // HyphenRunner &HyphenRunner::get()
// // {
// //     static HyphenRunner inst;
// //     return inst;
// // }

// // HyphenRunner::HyphenRunner()
// //     : taskHandle(nullptr), cmdQ(nullptr), eg(nullptr), hyphen(nullptr)
// // {
// // }

// // void HyphenRunner::begin(HyphenConnect *hc, int logLevel)
// // {
// //     hyphen = hc;
// //     // queue holds pointers to Command
// //     cmdQ = xQueueCreate(16, sizeof(Command *));
// //     eg = xEventGroupCreate();

// //     Log.infoln(F("[Runner] Starting on core %u"), xPortGetCoreID());
// //     xTaskCreatePinnedToCore(
// //         taskEntry,            // entry fn
// //         "HyphenRunner",       // for ps
// //         allocatedStack,       // new   6 KiB,                 // 8 KB stack
// //         this,                 // param
// //         tskIDLE_PRIORITY + 1, // high prio
// //         &taskHandle,
// //         0 // pin to core 0
// //     );

// //     enqueueSetup(logLevel);
// // }

// // bool HyphenRunner::isConnected() const
// // {
// //     EventBits_t bits = xEventGroupGetBits(eg);
// //     return (bits & BIT_TRANSPORT) && (bits & BIT_MQTT);
// // }

// // // --- enqueue implementations ---

// // bool HyphenRunner::enqueueSetup(int logLevel)
// // {
// //     Command *cmd = new Command();
// //     cmd->type = Command::SETUP;
// //     cmd->logLevel = logLevel;
// //     bool ok = xQueueSend(cmdQ, &cmd, 0) == pdTRUE;
// //     if (!ok)
// //         delete cmd;
// //     return ok;
// // }

// // bool HyphenRunner::enqueueSubscribe(const char *topic,
// //                                     std::function<void(const char *, const char *)> cb)
// // {
// //     Command *cmd = new Command();
// //     cmd->type = Command::SUBSCRIBE;
// //     cmd->topic = topic;
// //     cmd->cb = cb;
// //     bool ok = xQueueSend(cmdQ, &cmd, 0) == pdTRUE;
// //     if (!ok)
// //         delete cmd;
// //     return ok;
// // }

// // bool HyphenRunner::enqueueUnsubscribe(const char *topic)
// // {
// //     Command *cmd = new Command();
// //     cmd->type = Command::UNSUBSCRIBE;
// //     cmd->topic = topic;
// //     bool ok = xQueueSend(cmdQ, &cmd, 0) == pdTRUE;
// //     if (!ok)
// //         delete cmd;
// //     return ok;
// // }

// // bool HyphenRunner::enqueueFunction(const char *name,
// //                                    std::function<int(const char *)> fn)
// // {
// //     Command *cmd = new Command();
// //     cmd->type = Command::FUNCTION;
// //     cmd->name = name;
// //     cmd->fn = fn;
// //     bool ok = xQueueSend(cmdQ, &cmd, 0) == pdTRUE;
// //     if (!ok)
// //         delete cmd;
// //     return ok;
// // }

// // bool HyphenRunner::enqueueVariable(const char *name, void *ptr)
// // {
// //     Command *cmd = new Command();
// //     cmd->type = Command::VARIABLE;
// //     cmd->name = name;
// //     cmd->ptr = ptr;
// //     bool ok = xQueueSend(cmdQ, &cmd, 0) == pdTRUE;
// //     if (!ok)
// //         delete cmd;
// //     return ok;
// // }

// // bool HyphenRunner::enqueuePublish(const String &topic, const String &payload)
// // {
// //     Command *cmd = new Command();
// //     cmd->type = Command::PUBLISH;
// //     cmd->topic = topic;
// //     cmd->payload = payload;
// //     bool ok = xQueueSend(cmdQ, &cmd, 0) == pdTRUE;
// //     if (!ok)
// //         delete cmd;
// //     return ok;
// // }

// // void HyphenRunner::enqueueDisconnect()
// // {
// //     Command *cmd = new Command();
// //     cmd->type = Command::DISCONNECT;
// //     if (xQueueSend(cmdQ, &cmd, 0) != pdTRUE)
// //     {
// //         delete cmd;
// //     }
// // }

// // // task entry trampoline
// // void HyphenRunner::taskEntry(void *pv)
// // {
// //     static_cast<HyphenRunner *>(pv)->runTask();
// // }

// // void HyphenRunner::runTask()
// // {
// //     Command *cmdPtr = nullptr;

// //     // --- initial setup phase ---
// //     if (xQueueReceive(cmdQ, &cmdPtr, portMAX_DELAY) && cmdPtr->type == Command::SETUP)
// //     {
// //         Log.infoln(F("[Runner] Running connection manager.init()"));
// //         while (!hyphen->manager.init())
// //         {
// //             Log.infoln(F("[Runner] Running connection.init()"));
// //             while (!hyphen->connection.isConnected() && !hyphen->connection.init())
// //             {
// //                 Log.infoln(F("[Runner] connection::init() failed, retry in 500 ms"));
// //                 vTaskDelay(pdMS_TO_TICKS(500));
// //             }
// //             Log.infoln(F("[Runner] manager::init() failed, retry in 500 ms"));
// //             vTaskDelay(pdMS_TO_TICKS(500));
// //         }
// //         Log.infoln(F("[Runner] init() OK, signaling transport+MQTT"));
// //         xEventGroupSetBits(eg, BIT_TRANSPORT | BIT_MQTT);
// //         delete cmdPtr;
// //     }

// //     TickType_t lastReport = xTaskGetTickCount();
// //     const TickType_t reportInterval = pdMS_TO_TICKS(10000); // 10 000 ms
// //                                                             // match your stack size
// //     size_t maxUsedBytes = 0;
// //     // --- main service loop ---
// //     // hyphen->connection.init();
// //     for (;;)
// //     {
// //         // drain any commands
// //         while (xQueueReceive(cmdQ, &cmdPtr, 0))
// //         {
// //             switch (cmdPtr->type)
// //             {
// //             case Command::SUBSCRIBE:
// //                 hyphen->manager.subscribe(cmdPtr->topic.c_str(), cmdPtr->cb);
// //                 break;
// //             case Command::UNSUBSCRIBE:
// //                 hyphen->manager.unsubscribe(cmdPtr->topic.c_str());
// //                 break;
// //             case Command::FUNCTION:
// //                 hyphen->manager.function(cmdPtr->name.c_str(), cmdPtr->fn);
// //                 break;
// //             case Command::VARIABLE:
// //                 hyphen->manager.variable(cmdPtr->name.c_str(),
// //                                          (long *)cmdPtr->ptr);
// //                 break;
// //             case Command::PUBLISH:
// //                 hyphen->manager.publishTopic(cmdPtr->topic, cmdPtr->payload);
// //                 break;
// //             case Command::DISCONNECT:
// //                 hyphen->processor.disconnect();
// //                 break;
// //             default:
// //                 break;
// //             }
// //             delete cmdPtr;
// //         }

// //         // drive the library
// //         hyphen->manager.loop();
// //         TickType_t now = xTaskGetTickCount();

// //         if ((now - lastReport) >= reportInterval)
// //         {
// //             // how many words have *never* been used
// //             UBaseType_t freeWords = uxTaskGetStackHighWaterMark(NULL);
// //             size_t freeBytes = freeWords * sizeof(StackType_t);
// //             size_t usedBytes = allocatedStack - freeBytes;
// //             if (usedBytes > maxUsedBytes)
// //             {
// //                 maxUsedBytes = usedBytes;
// //             }

// //             Log.infoln(F("[Runner][Stats] free=%uB  used=%uB  maxUsed=%uB"),
// //                        (unsigned)freeBytes,
// //                        (unsigned)usedBytes,
// //                        (unsigned)maxUsedBytes);

// //             lastReport = now;
// //         }

// //         vTaskDelay(pdMS_TO_TICKS(10));
// //     }
// //     Log.errorln(F("[Runner] Task should never exit"));
// // }