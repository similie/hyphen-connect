/* HyphenRunner.h */
#ifndef HYPHEN_RUNNER_H
#define HYPHEN_RUNNER_H
#include "Ticker.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

class HyphenConnect;

class HyphenRunner
{
public:
  static HyphenRunner &get();
  void begin(HyphenConnect *hc);
  bool isAlive() const;
  // bool isConnected() const;
  void stop();
  // bool initRequested(); // you can set a flag to request init()
  void restart(HyphenConnect *hc);
  volatile uint32_t _lastAliveMs;
  bool isStuck(uint32_t timeoutMs) const;
  void loop();
  bool ready();

private:
  HyphenRunner();
  void begin();
  void processorLoops();
  TaskHandle_t manangerInitHandle = nullptr;
  void startManagerInitTask();
  static void managerInitTask(void *pv);
  bool initManager();
  volatile bool connectionStarted = false;
  volatile bool runConnection = false;
  unsigned long threadCheck = 0;
  static void taskEntry(void *pv);
  static void maintenanceCallback(void *pv);
  void runTask();
  TaskHandle_t loopHandle = nullptr;
  EventGroupHandle_t eg = nullptr;
  HyphenConnect *hyphen = nullptr;
  void startRunnerTask();
  void startMaintenanceTask();
  int allocatedStack = 8192;
  const uint32_t LAST_ALIVE_THRESHOLD = 1000 * 60 * 5; // 10 minutes
  volatile bool initialized = false;
  void rebuildConnection();
  void tick()
  {
    _lastAliveMs = millis();
  }
  /** Returns true if runTask() has looped in the last `timeoutMs` milliseconds */
};

#endif
