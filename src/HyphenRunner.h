#pragma once
#include <Arduino.h>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>

// forward
class HyphenConnect;

class HyphenRunner
{
public:
  static HyphenRunner &get();

  // now takes the wrapper pointer:
  void begin(HyphenConnect *hyphenPtr, int logLevel);

  bool isConnected() const;
  bool enqueueSetup(int logLevel);
  bool enqueueSubscribe(const char *topic, std::function<void(const char *, const char *)>);
  bool enqueueFunction(const char *name, std::function<int(const char *)>);
  bool enqueueVariable(const char *name, void *varPtr);
  bool enqueuePublish(const String &topic, const String &payload);
  void enqueueDisconnect();

private:
  HyphenRunner();
  static void taskEntry(void *pv);
  void runTask();

  TaskHandle_t taskHandle;
  QueueHandle_t cmdQ;
  EventGroupHandle_t eg;
  HyphenConnect *hyphen; // pointer to your wrapper

  struct Command
  {
    enum Type
    {
      SETUP,
      SUBSCRIBE,
      FUNCTION,
      VARIABLE,
      PUBLISH,
      DISCONNECT
    } type;
    int logLevel;
    String topic, payload, name;
    void *ptr;
    std::function<void(const char *, const char *)> cb;
    std::function<int(const char *)> fn;
  };
};