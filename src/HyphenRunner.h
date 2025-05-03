/* HyphenRunner.h */
#ifndef HYPHEN_RUNNER_H
#define HYPHEN_RUNNER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

class HyphenConnect;

class HyphenRunner
{
public:
  static HyphenRunner &get();
  void begin(HyphenConnect *hc, int logLevel);
  bool isAlive() const;
  bool isConnected() const;
  void stop();
  bool initRequested(); // you can set a flag to request init()
  void restart(HyphenConnect *hc, int logLevel);

private:
  HyphenRunner();
  static void taskEntry(void *pv);
  void runTask();

  TaskHandle_t taskHandle = nullptr;
  EventGroupHandle_t eg = nullptr;
  HyphenConnect *hyphen = nullptr;
  int allocatedStack = 8192;
  bool wantInit = false;
};

#endif
// #include <Arduino.h>
// #include <freertos/FreeRTOS.h>
// #include <freertos/task.h>
// #include <freertos/queue.h>
// #include <freertos/event_groups.h>
// #include "managers/LoggingManager.h"
// class HyphenConnect;

// enum class CommandType
// {
//   SETUP,
//   SUBSCRIBE,
//   UNSUBSCRIBE,
//   PUBLISH,
//   FUNCTION,
//   VARIABLE,
//   DISCONNECT
// };
// enum class VarType
// {
//   INT,
//   LONG,
//   STRING,
//   DOUBLE
// };

// struct Command
// {
//   CommandType type;
//   String topic;
//   String payload;
//   std::function<void(const char *, const char *)> cb;
//   std::function<int(const char *)> fn;
//   String name; // for FUNCTION or VARIABLE
//   // For VARIABLE commands
//   void *ptr = nullptr;
//   VarType varType;
//   int logLevel;

//   // synchronous result
//   QueueHandle_t replyQ = nullptr;
//   bool result;
// };

// class HyphenRunner
// {
// public:
//   static HyphenRunner &get();
//   void begin(HyphenConnect *hc, int logLevel);
//   bool isAlive() const;
//   bool isConnected() const;

//   bool enqueueSetup(int logLevel);
//   bool enqueueSubscribe(const char *topic, std::function<void(const char *, const char *)> cb);
//   bool enqueueUnsubscribe(const char *topic);
//   bool enqueuePublish(const String &topic, const String &payload, bool *outResult);
//   bool enqueuePublish(const String &topic, const String &payload);
//   bool enqueueFunction(const char *name, std::function<int(const char *)> fn);
//   bool enqueueVariable(const char *name, int *var);
//   bool enqueueVariable(const char *name, long *var);
//   bool enqueueVariable(const char *name, String *var);
//   bool enqueueVariable(const char *name, double *var);
//   void enqueueDisconnect();
//   void teardownRunnerTask();
//   TaskHandle_t getTaskHandle() const { return taskHandle; }

// private:
//   HyphenRunner();
//   static void taskEntry(void *pv) { static_cast<HyphenRunner *>(pv)->runTask(); }
//   void runTask();

//   TaskHandle_t taskHandle = nullptr;
//   QueueHandle_t cmdQ = nullptr;
//   EventGroupHandle_t eg = nullptr;
//   HyphenConnect *hyphen = nullptr;
//   const int allocatedStack = 8192;
// };

// #endif // HYPHEN_RUNNER_H

// #pragma once
// #include <Arduino.h>
// #include <functional>
// #include <freertos/FreeRTOS.h>
// #include <freertos/task.h>
// #include <freertos/queue.h>
// #include <freertos/event_groups.h>
// #include "managers/LoggingManager.h"

// // forward the wrapper
// class HyphenConnect;

// class HyphenRunner
// {
// public:
//   static HyphenRunner &get();

//   // point at your HyphenConnect instance, and log‐level for initial bring-up
//   void begin(HyphenConnect *hc, int logLevel = LOG_LEVEL_SILENT);

//   // true once transport (Wi-Fi or cellular) + MQTT are up
//   bool isConnected() const;

//   // enqueue commands; these never block the caller
//   bool enqueueSetup(int logLevel);
//   bool enqueueSubscribe(const char *topic,
//                         std::function<void(const char *, const char *)> cb);
//   bool enqueueFunction(const char *name,
//                        std::function<int(const char *)> fn);
//   bool enqueueVariable(const char *name, void *varPtr);
//   bool enqueuePublish(const String &topic, const String &payload);
//   void enqueueDisconnect();
//   bool enqueueUnsubscribe(const char *topic);
//   // expose the internal FreeRTOS task handle
//   TaskHandle_t getTaskHandle() const { return taskHandle; }
//   bool isAlive() const
//   {
//     if (taskHandle == nullptr)
//       return false;
//     eTaskState state = eTaskGetState(taskHandle);
//     return state != eDeleted && state != eInvalid;
//   }

// private:
//   HyphenRunner();
//   ~HyphenRunner() = default;
//   HyphenRunner(const HyphenRunner &) = delete;
//   HyphenRunner &operator=(const HyphenRunner &) = delete;
//   const size_t allocatedStack = 4096 * 4;
//   static void taskEntry(void *pv);
//   void runTask();

//   // heap-allocated command
//   struct Command
//   {
//     enum Type
//     {
//       SETUP,
//       SUBSCRIBE,
//       UNSUBSCRIBE,
//       FUNCTION,
//       VARIABLE,
//       PUBLISH,
//       DISCONNECT
//     } type;
//     int logLevel;
//     String topic;
//     String payload;
//     String name;
//     void *ptr;
//     std::function<void(const char *, const char *)> cb;
//     std::function<int(const char *)> fn;
//     // default constructor
//     Command() : type(SETUP), logLevel(0), ptr(nullptr) {}
//   };

//   TaskHandle_t taskHandle;
//   QueueHandle_t cmdQ; // holds Command*
//   EventGroupHandle_t eg;
//   HyphenConnect *hyphen;
// };